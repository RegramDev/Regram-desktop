#include "regram/nagram/sync/service.h"

#include "regram/nagram/core/exchange.h"

#include "base/unixtime.h"
#include "core/version.h"
#include "main/main_session.h"

namespace Nagram::Sync {
namespace {

struct Entry {
	base::weak_ptr<Main::Session> session;
	Service *service = nullptr;
};

[[nodiscard]] std::vector<Entry> &Entries() {
	static auto result = std::vector<Entry>();
	std::erase_if(result, [](const Entry &entry) {
		return !entry.session;
	});
	return result;
}

[[nodiscard]] Error FromBackend(BackendError error) {
	switch (error) {
	case BackendError::None: return Error::None;
	case BackendError::Search: return Error::Search;
	case BackendError::Download: return Error::Download;
	case BackendError::TooLarge: return Error::TooLarge;
	case BackendError::Upload:
	case BackendError::Send: return Error::Upload;
	}
	Unexpected("Sync backend error.");
}

[[nodiscard]] Error FromEnvelope(EnvelopeError error) {
	switch (error) {
	case EnvelopeError::None: return Error::None;
	case EnvelopeError::TooLarge: return Error::TooLarge;
	case EnvelopeError::Damaged: return Error::Damaged;
	case EnvelopeError::Foreign: return Error::Foreign;
	case EnvelopeError::Newer: return Error::Newer;
	}
	Unexpected("Sync envelope error.");
}

} // namespace

Service::Service(not_null<Main::Session*> session)
: _session(session)
, _backend(MakeSavedMessagesBackend(session))
, _autoTimer([=] { autoRun(); })
, _autoTimeout([=] {
	cancel();
	autoFinish(Error::Timeout);
}) {
}

Service::~Service() = default;

Service &Service::For(not_null<Main::Session*> session) {
	if (const auto found = Find(session)) {
		return *found;
	}
	const auto service = session->lifetime().make_state<Service>(session);
	Entries().push_back({ base::make_weak(session), service });
	return *service;
}

Service *Service::Find(not_null<Main::Session*> session) {
	for (const auto &entry : Entries()) {
		if (entry.session.get() == session) {
			return entry.service;
		}
	}
	return nullptr;
}

bool Service::busy() const {
	return _busy;
}

std::optional<State> Service::state() const {
	const auto parsed = ParseState(ForAccount(_session).Get(kState));
	return (parsed && parsed->user == _session->userId().bare)
		? parsed
		: std::nullopt;
}

void Service::save(
		quint64 messageId,
		const QByteArray &hash,
		qint64 updatedAt) {
	Expects(ForAccount(_session).Set(kState, SerializeState({
		.user = _session->userId().bare,
		.messageId = messageId,
		.hash = hash,
		.updatedAt = updatedAt,
	})));
}

void Service::cancel() {
	_backend->cancel();
	_busy = false;
	_autoRunning = false;
	_autoTimeout.cancel();
}

void Service::setAuto(bool enabled) {
	if (_auto == enabled) {
		return;
	}
	_auto = enabled;
	_autoLifetime.destroy();
	_autoTimer.cancel();
	_autoIssue = Error::None;
	if (!enabled) {
		if (_autoRunning) {
			cancel();
		}
		return;
	}
	ForDevice().changes(
	) | rpl::on_next([=] {
		autoSchedule(kAutoDebounceSeconds);
	}, _autoLifetime);
	autoSchedule(kAutoDebounceSeconds);
}

rpl::producer<Error> Service::autoIssueValue() const {
	return _autoIssue.value();
}

void Service::autoSchedule(qint64 seconds) {
	_autoTimer.callOnce(seconds * crl::time(1000));
}

void Service::autoRun() {
	if (!_auto) {
		return;
	} else if (_busy) {
		autoSchedule(kAutoDebounceSeconds);
		return;
	}
	const auto exported = Exchange::Export(
		ForDevice(),
		RegisteredOptions(),
		ExchangeTarget::Sync);
	if (!exported.invalidKeys.isEmpty()) {
		_autoIssue = Error::LocalInvalid;
		return;
	}
	const auto known = state();
	const auto now = qint64(base::unixtime::now());
	const auto plan = PlanAuto(
		now,
		_autoAttempt,
		PayloadHash(exported.data),
		known ? known->hash : QByteArray());
	if (plan.step == AutoStep::Skip) {
		_autoIssue = Error::None;
		return;
	} else if (plan.step == AutoStep::Wait) {
		autoSchedule(plan.wait);
		return;
	}
	_autoAttempt = now;
	_autoRunning = true;
	_autoTimeout.callOnce(kAutoTimeoutSeconds * crl::time(1000));
	check([=](Checked checked) {
		if (checked.error != Error::None) {
			autoFinish(checked.error);
		} else if (!checked.found || checked.action == Action::Upload) {
			upload([=](Error error) { autoFinish(error); });
		} else if (checked.action == Action::UpToDate) {
			autoFinish(Error::None);
		} else {
			autoFinish(Error::RemoteChanged);
		}
	});
}

void Service::autoFinish(Error error) {
	_autoRunning = false;
	_autoTimeout.cancel();
	_autoIssue.force_assign(error);
}

void Service::check(Fn<void(Checked)> done) {
	if (_busy) {
		done({ .error = Error::Busy });
		return;
	}
	const auto exported = Exchange::Export(
		ForDevice(),
		RegisteredOptions(),
		ExchangeTarget::Sync);
	if (!exported.invalidKeys.isEmpty()) {
		done({ .error = Error::LocalInvalid });
		return;
	}
	const auto local = PayloadHash(exported.data);
	const auto known = state();
	_busy = true;
	_remoteId = 0;
	_remoteIds.clear();
	_backend->fetch(known ? known->messageId : 0, [=](Fetched fetched) {
		_busy = false;
		_remoteIds = fetched.all;
		if (fetched.error != BackendError::None) {
			done({
				.error = FromBackend(fetched.error),
				.found = (fetched.error == BackendError::TooLarge),
			});
			return;
		} else if (!fetched.found) {
			done({});
			return;
		}
		_remoteId = fetched.id;
		const auto decoded = DecodeEnvelope(fetched.data);
		if (decoded.error != EnvelopeError::None) {
			done({ .error = FromEnvelope(decoded.error), .found = true });
			return;
		}
		const auto &remote = decoded.value;
		const auto action = Decide(
			local,
			known ? known->hash : QByteArray(),
			remote.hash);
		if (action == Action::UpToDate
			&& (!known
				|| known->hash != remote.hash
				|| known->messageId != fetched.id)) {
			save(fetched.id, remote.hash, base::unixtime::now());
		}
		done({ .found = true, .action = action, .remote = remote });
	});
}

void Service::upload(Fn<void(Error)> done) {
	if (_busy) {
		done(Error::Busy);
		return;
	}
	const auto exported = Exchange::Export(
		ForDevice(),
		RegisteredOptions(),
		ExchangeTarget::Sync);
	if (!exported.invalidKeys.isEmpty()) {
		done(Error::LocalInvalid);
		return;
	}
	const auto now = qint64(base::unixtime::now());
	const auto hash = PayloadHash(exported.data);
	const auto data = EncodeEnvelope(
		exported.data,
		now,
		QString::fromLatin1(AppVersionStr));
	_busy = true;
	_backend->upload(data, [=](Uploaded uploaded) {
		_busy = false;
		if (uploaded.error != BackendError::None) {
			done(FromBackend(uploaded.error));
			return;
		}
		const auto known = state();
		const auto old = SupersededBackups(
			base::take(_remoteIds),
			known ? known->messageId : 0,
			uploaded.id);
		save(uploaded.id, hash, now);
		_remoteId = uploaded.id;
		_backend->remove(old);
		done(Error::None);
	});
}

void Service::removeRemote(Fn<void(Error)> done) {
	if (_busy) {
		done(Error::Busy);
		return;
	}
	const auto known = state();
	_busy = true;
	_backend->fetch(known ? known->messageId : 0, [=](Fetched fetched) {
		_busy = false;
		if (fetched.error == BackendError::Search) {
			done(Error::Search);
			return;
		}
		_backend->remove(fetched.all);
		_remoteId = 0;
		_remoteIds.clear();
		Expects(ForAccount(_session).Set(kState, QByteArray()));
		done(Error::None);
	});
}

void Service::applied(const Envelope &remote) {
	save(_remoteId, remote.hash, base::unixtime::now());
}

} // namespace Nagram::Sync
