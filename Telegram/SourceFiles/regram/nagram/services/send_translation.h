#pragma once

#include "ui/widgets/menu/menu_add_action_callback.h"

#include <rpl/producer.h>

class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::SendTranslation {

void AddPeerMenu(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Window::SessionController*> controller,
	PeerData *peer);
[[nodiscard]] rpl::producer<int> ChatsCountValue(
	not_null<Main::Session*> session);
void ClearChats(not_null<Main::Session*> session);

} // namespace Nagram::SendTranslation
