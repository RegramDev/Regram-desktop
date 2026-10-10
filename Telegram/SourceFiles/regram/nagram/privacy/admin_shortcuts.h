#pragma once

#include "ui/widgets/menu/menu_add_action_callback.h"

class PeerData;
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Privacy {

void AddAdminShortcuts(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

} // namespace Nagram::Privacy
