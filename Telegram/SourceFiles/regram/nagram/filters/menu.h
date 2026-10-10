#pragma once

#include "ui/widgets/menu/menu_add_action_callback.h"

class HistoryItem;
class PeerData;
namespace Data { class ForumTopic; }
namespace Ui { class PopupMenu; }
namespace Window { class SessionController; }

namespace Nagram::Filters {

void InsertAuthorAction(
	Ui::PopupMenu *menu,
	HistoryItem *item,
	Window::SessionController *controller);
void AddScopeAction(
	const Ui::Menu::MenuCallback &addAction,
	not_null<Window::SessionController*> controller,
	PeerData *peer,
	Data::ForumTopic *topic);

} // namespace Nagram::Filters
