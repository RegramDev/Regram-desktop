#pragma once

namespace Dialogs {
class Key;
} // namespace Dialogs
namespace Ui {
class RpWidget;
} // namespace Ui
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Chats {

[[nodiscard]] int LayoutChatTools(
	not_null<Ui::RpWidget*> bar,
	not_null<Window::SessionController*> controller,
	const Dialogs::Key &key,
	bool allowed,
	int right,
	int top);

} // namespace Nagram::Chats
