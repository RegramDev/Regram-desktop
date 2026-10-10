#pragma once

#include "ui/widgets/menu/menu_add_action_callback.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

#include "settings/settings_type.h"

namespace Nagram {

[[nodiscard]] Settings::Type ComposeId();
void AddComposePeerMenu(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

} // namespace Nagram
