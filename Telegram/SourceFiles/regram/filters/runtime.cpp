#include "regram/filters/runtime.h"

#include "base/weak_ptr.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/filters/reveal.h"
#include "regram/filters/search_text.h"
#include "storage/storage_account.h"

#include <QtCore/QCache>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <algorithm>
#include <map>

namespace Regram::Filters {
namespace {

struct Cached {
	FullMsgId id;
	std::optional<PeerAddress> peer;
	QString text;
	bool hidden = false;
};

struct State {
	explicit State(not_null<Main::Session*> session);
	base::weak_ptr<Main::Session> guard;
	StoredRules stored;
	Engine engine;
	QCache<quintptr, Cached> results;
	bool loaded = false;
	QByteArray hiddenRaw;
	QSet<uint64> hiddenSenders;
	QByteArray disabledRaw;
	QSet<uint64> disabledChats;
	rpl::event_stream<> updates;
};

State::State(not_null<Main::Session*> session)
: guard(base::make_weak(session))
, results(4 * 1024 * 1024) {
}

QByteArray Raw(not_null<Main::Session*> session) {
	return session->local().readPref<QByteArray>(kPortableRules.key);
}

State &StateFor(not_null<Main::Session*> session) {
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	auto found = states.find(session);
	if (found == states.end() || !found->second->guard) {
		std::erase_if(states, [](const auto &entry) {
			return !entry.second->guard;
		});
		found = states.emplace(session, std::make_unique<State>(session)).first;
	}
	auto &state = *found->second;
	const auto raw = Raw(session);
	if (!state.loaded || state.stored.raw != raw) {
		state.loaded = true;
		state.results.clear();
		auto imported = raw.isEmpty() ? ImportResult() : ImportStoredRules(raw);
		state.stored = {
			.raw = raw,
			.rules = std::move(imported.rules),
			.problems = std::move(imported.problems),
			.error = std::move(imported.error),
		};
		if (!state.engine.setRules(state.stored.rules)) {
			state.stored.error = tr::lng_nagram_filter_invalid(tr::now);
		}
	}
	const auto hidden = Nagram::ForAccount(session).Get(kHiddenSenders);
	if (state.hiddenRaw != hidden) {
		state.hiddenRaw = hidden;
		state.hiddenSenders.clear();
		for (const auto &entry : QJsonDocument::fromJson(hidden).array()) {
			state.hiddenSenders.insert(entry.toString().toULongLong());
		}
	}
	const auto disabled = Nagram::ForAccount(session).Get(kDisabledChats);
	if (state.disabledRaw != disabled) {
		state.disabledRaw = disabled;
		state.disabledChats.clear();
		for (const auto &entry : QJsonDocument::fromJson(disabled).array()) {
			state.disabledChats.insert(entry.toString().toULongLong());
		}
	}
	return state;
}

std::optional<PeerAddress> Address(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	if (!peer->isUser() && !peer->isChat() && !peer->isChannel()) {
		return std::nullopt;
	}
	return PeerAddress{
		.kind = peer->isUser() ? PeerKind::User
			: peer->isChat() ? PeerKind::Group : PeerKind::Channel,
		.id = peer->id.value & PeerId::kChatTypeMask,
		.thread = peer->isForum() ? uint64(item->topicRootId().bare) : 0,
	};
}

} // namespace

bool ValidateStored(const QByteArray &raw) {
	return raw.isEmpty() || ImportStoredRules(raw).error.isEmpty();
}

StoredRules Read(not_null<Main::Session*> session) {
	return StateFor(session).stored;
}

void Migrate(not_null<Main::Session*> session) {
	const auto legacy = "nagram.regramFiltersEnabled";
	if (session->local().readPref<QByteArray>(legacy) != "0") {
		return;
	}
	const auto current = Read(session);
	auto rules = current.rules;
	for (auto &rule : rules) {
		rule.enabled = false;
	}
	const auto error = current.error.isEmpty()
		? Write(session, current.raw, rules) : QString();
	if (error.isEmpty()) {
		session->local().clearPref(legacy);
	}
}

QString Write(
		not_null<Main::Session*> session,
		const QByteArray &expected,
		const std::vector<Rule> &rules) {
	if (Raw(session) != expected) {
		return tr::lng_nagram_config_changed_error(tr::now);
	}
	const auto ios = ExportIosRules(rules);
	const auto raw = rules.empty() ? QByteArray()
		: ios.error.isEmpty() ? ios.data : ExportStoredRules(rules);
	const auto checked = raw.isEmpty() ? ImportResult() : ImportStoredRules(raw);
	if (!checked.error.isEmpty()) {
		return checked.error;
	}
	for (const auto &problem : checked.problems) {
		if (ranges::any_of(rules, [&](const Rule &rule) {
			return rule.enabled && rule.id == problem.ruleId;
		})) {
			return QString::fromStdString(problem.reason);
		}
	}
	if (!Nagram::ForAccount(session).Set(kPortableRules, raw)) {
		return tr::lng_nagram_filter_invalid(tr::now);
	}
	return QString();
}

bool Hidden(not_null<HistoryItem*> item, bool displayOverrides) {
	const auto peer = item->history()->peer;
	if (!item->isRegular()
		|| (displayOverrides && TemporarilyRevealed(peer))) {
		return false;
	}
	auto &state = StateFor(&peer->session());
	if (!item->from()->isSelf()
		&& state.hiddenSenders.contains(SerializePeerId(item->from()->id))) {
		return true;
	}
	if (item->out() || state.disabledChats.contains(SerializePeerId(peer->id))) {
		return false;
	}
	if (!state.stored.error.isEmpty() || state.stored.rules.empty()) {
		return false;
	}
	const auto address = Address(item);
	const auto text = SearchText(item);
	const auto key = reinterpret_cast<quintptr>(item.get());
	if (const auto cached = state.results.object(key)
		; cached && cached->id == item->fullId()
		&& cached->peer == address && cached->text == text) {
		return cached->hidden;
	}
	const auto result = state.engine.evaluate(
		std::u16string_view(
			reinterpret_cast<const char16_t*>(text.utf16()), text.size()),
		address);
	state.results.insert(key, new Cached{
		.id = item->fullId(),
		.peer = address,
		.text = text,
		.hidden = result.hidden,
	}, std::max(1, int(text.size() * sizeof(char16_t) + sizeof(Cached))));
	return result.hidden;
}

bool ValidDisabledChats(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	} else if (raw.size() > kMaxScopes * 24) {
		return false;
	}
	const auto document = QJsonDocument::fromJson(raw);
	if (!document.isArray() || document.array().size() > kMaxScopes) {
		return false;
	}
	return ranges::all_of(document.array(), [](const auto &entry) {
		return entry.isString() && ParseServerId(entry.toString().toStdString()).has_value();
	});
}

bool EnabledForPeer(not_null<PeerData*> peer) {
	return !StateFor(&peer->session()).disabledChats.contains(SerializePeerId(peer->id));
}

bool SetEnabledForPeer(not_null<PeerData*> peer, bool enabled) {
	const auto session = &peer->session();
	const auto raw = Nagram::ForAccount(session).Get(kDisabledChats);
	if (!ValidDisabledChats(raw)) {
		return false;
	}
	auto entries = QJsonDocument::fromJson(raw).array();
	const auto id = QString::number(SerializePeerId(peer->id));
	const auto index = entries.toVariantList().indexOf(id);
	if (enabled && index >= 0) {
		entries.removeAt(index);
	} else if (!enabled && index < 0) {
		if (entries.size() == kMaxScopes) {
			return false;
		}
		entries.push_back(id);
	}
	return Nagram::ForAccount(session).Set(kDisabledChats,
		QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

rpl::producer<> Updates(not_null<Main::Session*> session) {
	return rpl::merge(
		StateFor(session).updates.events(),
		Nagram::ForAccount(session).changes()
			| rpl::filter([](std::string_view key) {
				return key == kPortableRules.key
					|| key == kDisabledChats.key || key == kHiddenSenders.key;
			}) | rpl::to_empty);
}

void NotifyChanged(not_null<Main::Session*> session) {
	StateFor(session).updates.fire({});
}

std::vector<uint64> HiddenSenders(not_null<Main::Session*> session) {
	const auto &senders = StateFor(session).hiddenSenders;
	auto result = std::vector<uint64>(senders.begin(), senders.end());
	std::sort(result.begin(), result.end());
	return result;
}

bool SenderHidden(not_null<PeerData*> peer) {
	return !peer->isSelf()
		&& StateFor(&peer->session()).hiddenSenders.contains(SerializePeerId(peer->id));
}

bool SetSenderHidden(not_null<PeerData*> peer, bool hidden) {
	if (peer->isSelf() || !peer->isUser()) {
		return false;
	}
	const auto session = &peer->session();
	auto senders = StateFor(session).hiddenSenders;
	const auto id = SerializePeerId(peer->id);
	if (hidden) {
		if (!senders.contains(id) && senders.size() >= kMaxScopes) {
			return false;
		}
		senders.insert(id);
	} else {
		senders.remove(id);
	}
	auto entries = QJsonArray();
	for (const auto sender : senders) {
		entries.push_back(QString::number(sender));
	}
	return Nagram::ForAccount(session).Set(kHiddenSenders,
		QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

bool SuppressNotification(not_null<HistoryItem*> item) {
	return item->isRegular() && !item->out()
		&& (SenderHidden(item->from())
			|| (Nagram::ForAccount(&item->history()->session()).Get(kSuppressNotifications)
				&& Hidden(item, false)));
}

} // namespace Regram::Filters
