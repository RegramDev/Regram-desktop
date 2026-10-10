#pragma once

#include "regram/nagram/sync/backend.h"
#include "regram/nagram/sync/model.h"
#include "base/timer.h"
#include "base/weak_ptr.h"

namespace Nagram::Sync {

enum class Error {
	None,
	Busy,
	LocalInvalid,
	Search,
	Download,
	TooLarge,
	Upload,
	Damaged,
	Foreign,
	Newer,
	Timeout,
	RemoteChanged,
};

struct Checked {
	Error error = Error::None;
	bool found = false;
	Action action = Action::Upload;
	Envelope remote;
};

class Service final : public base::has_weak_ptr {
public:
	explicit Service(not_null<Main::Session*> session);
	~Service();

	[[nodiscard]] static Service &For(not_null<Main::Session*> session);
	[[nodiscard]] static Service *Find(not_null<Main::Session*> session);

	[[nodiscard]] bool busy() const;
	void check(Fn<void(Checked)> done);
	void upload(Fn<void(Error)> done);
	void removeRemote(Fn<void(Error)> done);
	void applied(const Envelope &remote);
	void cancel();

	void setAuto(bool enabled);
	[[nodiscard]] rpl::producer<Error> autoIssueValue() const;

private:
	void autoSchedule(qint64 seconds);
	void autoRun();
	void autoFinish(Error error);

	[[nodiscard]] std::optional<State> state() const;
	void save(quint64 messageId, const QByteArray &hash, qint64 updatedAt);

	const not_null<Main::Session*> _session;
	const std::unique_ptr<Backend> _backend;
	bool _busy = false;
	quint64 _remoteId = 0;
	std::vector<quint64> _remoteIds;

	bool _auto = false;
	bool _autoRunning = false;
	qint64 _autoAttempt = 0;
	rpl::variable<Error> _autoIssue = Error::None;
	base::Timer _autoTimer;
	base::Timer _autoTimeout;
	rpl::lifetime _autoLifetime;

};

} // namespace Nagram::Sync
