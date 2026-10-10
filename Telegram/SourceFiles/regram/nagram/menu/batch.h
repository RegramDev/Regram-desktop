#pragma once

#include "data/data_types.h"
#include "base/basic_types.h"

class HistoryItem;
namespace Ui {
class PopupMenu;
} // namespace Ui
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Menu {

void InsertBatchActions(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller,
	MessageIdsList selected,
	Fn<void(HistoryItem*)> select);

} // namespace Nagram::Menu
