#pragma once

#include "regram/nagram/chats/options.h"
#include "data/data_types.h"
#include "lang/lang_keys.h"
#include "styles/style_menu_icons.h"

class History;
namespace Main {
class Session;
} // namespace Main
class PeerListRow;
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Chats {

// Outside the peer id space and the folder type flag ids.
inline constexpr auto kRecentFilterRowId = uint64(1) << 62;

void WatchRecentChats(not_null<Window::SessionController*> controller);
void ForEachRecentShareTarget(
	not_null<Main::Session*> session,
	Fn<void(not_null<History*>)> callback);
void ForEachRecentChat(
	not_null<Main::Session*> session,
	Fn<void(not_null<History*>)> callback);
[[nodiscard]] std::unique_ptr<PeerListRow> MakeRecentFilterRow();
[[nodiscard]] bool RecentInFolder(
	not_null<History*> history,
	FilterId folderId);
[[nodiscard]] bool RecentFolderEnabled(
	not_null<Main::Session*> session,
	FilterId folderId);
void SetRecentFolderEnabled(
	not_null<Main::Session*> session,
	FilterId folderId,
	bool enabled);
void ShowRecentChats(not_null<Window::SessionController*> controller);

template <typename AddAction>
void AddRecentChatsMenuItem(
		AddAction &&addAction,
		not_null<Window::SessionController*> controller) {
	if (!ForDevice().Get(kRecentChats)) {
		return;
	}
	addAction(
		tr::lng_nagram_recent_chats(),
		{ &st::menuIconRestore },
		u"recentChats"_q
	)->setClickedCallback([=] {
		ShowRecentChats(controller);
	});
}

} // namespace Nagram::Chats
