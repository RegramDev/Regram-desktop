#pragma once

#include "regram/nagram/services/auto_translate_model.h"
#include "ui/widgets/menu/menu_add_action_callback.h"

class History;
class PeerData;
namespace Main {
class Session;
} // namespace Main
namespace Ui {
class GenericBox;
} // namespace Ui
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::AutoTranslate {

[[nodiscard]] Mode For(not_null<History*> history);
[[nodiscard]] bool Enabled(not_null<History*> history);
[[nodiscard]] rpl::producer<State> StateValue(not_null<History*> history);
void AddPeerMenu(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Window::SessionController*> controller,
	PeerData *peer);

[[nodiscard]] QString ModeLabel(Mode mode, const QString &inherit);
[[nodiscard]] rpl::producer<QString> AboutValue(
	not_null<Main::Session*> session);
void DeviceModeBox(not_null<Ui::GenericBox*> box);
void AccountModeBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void ChatsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);

} // namespace Nagram::AutoTranslate
