#pragma once

class HistoryItem;
namespace Main {
class Session;
} // namespace Main
namespace Ui {
class PopupMenu;
} // namespace Ui
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Filters {

[[nodiscard]] bool MessageHidden(not_null<HistoryItem*> item);
[[nodiscard]] int HiddenMessagesCount(not_null<Main::Session*> session);
void ClearHiddenMessages(not_null<Main::Session*> session);
void InsertHideMessageAction(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller);

} // namespace Nagram::Filters
