#pragma once

#include "regram/nagram/menu/model.h"
#include "data/data_types.h"
#include "base/basic_types.h"

class QAction;
class HistoryItem;
namespace Window {
class SessionController;
} // namespace Window
namespace Ui {
class PopupMenu;
} // namespace Ui
namespace style {
struct PopupMenu;
} // namespace style

namespace Nagram::Menu {

[[nodiscard]] const style::PopupMenu &MessageMenuStyle();
void Tag(QAction *action, ActionId id);
[[nodiscard]] int EndPosition(not_null<Ui::PopupMenu*> menu);
[[nodiscard]] bool Shown(ActionId id);
void Apply(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller,
	MessageIdsList selected,
	Fn<void(HistoryItem*)> select); // nullptr: between selected, else all

} // namespace Nagram::Menu
