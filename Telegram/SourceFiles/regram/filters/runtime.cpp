#include "regram/filters/runtime.h"
#include "regram/filters/search_text.h"
#include "regram/nagram/filters/view.h"
#include "regram/ayu/features/filters/filters_controller.h"

#include "base/weak_ptr.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/storage_account.h"

#include <QtCore/QCache>

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
		auto imported = raw.isEmpty() ? ImportResult() : ImportRules(raw);
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
	return raw.isEmpty() || ImportRules(raw).error.isEmpty();
}

StoredRules Read(not_null<Main::Session*> session) {
	return StateFor(session).stored;
}

QString Write(
		not_null<Main::Session*> session,
		const QByteArray &expected,
		const std::vector<Rule> &rules) {
	if (Raw(session) != expected) {
		return tr::lng_nagram_config_changed_error(tr::now);
	}
	const auto raw = rules.empty() ? QByteArray() : ExportRules(rules);
	const auto checked = raw.isEmpty() ? ImportResult() : ImportRules(raw);
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

bool Hidden(not_null<HistoryItem*> item) {
	auto &state = StateFor(&item->history()->session());
	if (!state.stored.error.isEmpty() || state.stored.rules.empty()) {
		return false;
	}
	const auto peer = Address(item);
	const auto text = SearchText(item);
	const auto key = reinterpret_cast<quintptr>(item.get());
	if (const auto cached = state.results.object(key)
		; cached && cached->id == item->fullId()
		&& cached->peer == peer && cached->text == text) {
		return cached->hidden;
	}
	const auto result = state.engine.evaluate(
		std::u16string_view(
			reinterpret_cast<const char16_t*>(text.utf16()), text.size()),
		peer);
	state.results.insert(key, new Cached{
		.id = item->fullId(),
		.peer = peer,
		.text = text,
		.hidden = result.hidden,
	}, std::max(1, int(text.size() * sizeof(char16_t) + sizeof(Cached))));
	return result.hidden;
}

bool SuppressNotification(not_null<HistoryItem*> item) {
	return Nagram::ForAccount(&item->history()->session()).Get(kSuppressNotifications)
		&& (Nagram::Filters::Hidden(item) || FiltersController::filtered(item));
}

} // namespace Regram::Filters
