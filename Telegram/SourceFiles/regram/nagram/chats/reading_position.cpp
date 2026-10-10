#include "regram/nagram/chats/reading_position.h"

#include "regram/nagram/chats/options.h"
#include "base/timer.h"
#include "data/data_peer.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"

namespace Nagram::Chats {
namespace {

constexpr auto kSaveInterval = 15 * crl::time(1000);

using Positions = std::vector<std::pair<PeerId, MsgId>>;

Positions Read(not_null<Main::Session*> session) {
	auto result = Positions();
	const auto value = ForAccount(session).Get(kReadingPositions);
	for (const auto &entry : value.split(u',', Qt::SkipEmptyParts)) {
		const auto parts = entry.split(u':');
		if (parts.size() == 2) {
			result.emplace_back(
				PeerId(parts[0].toULongLong()),
				MsgId(parts[1].toLongLong()));
		}
	}
	return result;
}

void Write(not_null<Main::Session*> session, const Positions &positions) {
	auto parts = QStringList();
	for (const auto &[peer, msg] : positions) {
		parts.push_back(QString::number(peer.value)
			+ u':'
			+ QString::number(msg.bare));
	}
	const auto value = parts.join(u',');
	auto &options = ForAccount(session);
	if (options.Get(kReadingPositions) != value) {
		Expects(options.Set(kReadingPositions, value));
	}
}

void Save(History *history) {
	if (!history || !ForDevice().Get(kSaveReadingPosition)) {
		return;
	}
	const auto session = &history->session();
	const auto peerId = history->peer->id;
	const auto view = history->scrollTopItem;
	if (!view) {
		return;
	}
	const auto msgId = view ? view->data()->id : MsgId();
	auto positions = Read(session);
	positions.erase(ranges::remove(
		positions,
		peerId,
		&std::pair<PeerId, MsgId>::first), positions.end());
	if (IsServerMsgId(msgId)) {
		positions.emplace(positions.begin(), peerId, msgId);
		if (positions.size() > kMaximumReadingPositions) {
			positions.resize(kMaximumReadingPositions);
		}
	}
	Write(session, positions);
}

} // namespace

void WatchReadingPositions(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	controller->activeChatValue(
	) | rpl::map([](const Dialogs::Key &key) {
		return key.history();
	}) | rpl::combine_previous(
	) | rpl::on_next([](History *previous, History *) {
		Save(previous);
	}, controller->lifetime());

	const auto timer = controller->lifetime().make_state<base::Timer>([=] {
		Save(controller->activeChatCurrent().history());
	});
	timer->callEach(kSaveInterval);

	ForDevice().Value(kSaveReadingPosition) | rpl::filter([](bool enabled) {
		return !enabled;
	}) | rpl::on_next([=] {
		Write(session, {});
	}, controller->lifetime());
}

MsgId RestoreReadingPosition(not_null<History*> history, MsgId showAtMsgId) {
	if (showAtMsgId != ShowAtUnreadMsgId
		|| !ForDevice().Get(kSaveReadingPosition)
		|| history->scrollTopItem
		|| history->unreadCount() > 0) {
		return showAtMsgId;
	}
	for (const auto &[peer, msg] : Read(&history->session())) {
		if (peer == history->peer->id) {
			return msg;
		}
	}
	return showAtMsgId;
}

} // namespace Nagram::Chats
