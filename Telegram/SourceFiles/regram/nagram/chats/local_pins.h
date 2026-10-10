#pragma once

#include "base/basic_types.h"
#include "ui/widgets/menu/menu_add_action_callback.h"

class History;

namespace Dialogs {
class Entry;
} // namespace Dialogs

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Chats {

[[nodiscard]] uint64 LocalPinSortKey(
	const Dialogs::Entry &entry,
	FilterId filterId,
	uint64 original);
[[nodiscard]] bool ShowsPinnedIcon(
	not_null<const Dialogs::Entry*> entry,
	FilterId filterId);
[[nodiscard]] bool TryLocalPin(
	not_null<Window::SessionController*> controller,
	not_null<History*> history);
[[nodiscard]] bool LocalUnpin(
	not_null<Dialogs::Entry*> entry,
	const Fn<void()> &onToggled);
[[nodiscard]] bool AddLocalUnpinAction(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Dialogs::Entry*> entry,
	FilterId filterId);

[[nodiscard]] rpl::producer<int> LocalPinsCountValue(
	not_null<Main::Session*> session);
void LocalPinsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);

} // namespace Nagram::Chats
