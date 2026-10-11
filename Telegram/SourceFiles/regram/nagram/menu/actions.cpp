#include "regram/nagram/menu/actions.h"
#include "regram/nagram/menu/repeat.h"
#include "regram/nagram/menu/batch.h"
#include "regram/nagram/menu/media.h"
#include "regram/nagram/menu/download.h"
#include "regram/nagram/menu/rating.h"
#include "regram/nagram/menu/message_tools.h"
#include "regram/nagram/menu/reading.h"
#include "regram/nagram/filters/menu.h"
#include "regram/nagram/services/summary.h"
#include "regram/nagram/snapshot/snapshot.h"

#include "ui/widgets/menu/menu.h"
#include "ui/widgets/menu/menu_item_base.h"
#include "ui/widgets/popup_menu.h"
#include "styles/style_nagram_compose.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

#include <QtGui/QAction>
#include <QtGui/QGuiApplication>

namespace Nagram::Menu {
namespace {

constexpr auto kActionIdProperty = "nagramMenuActionId";
constexpr auto kRootProperty = "regramMessageMenuRoot";
constexpr auto kSubmenuProperty = "regramMessageMenuSubmenu";

void Reorder(not_null<Ui::PopupMenu*> menu) {
	const auto raw = ForDevice().Get(kMenuOrder);
	if (raw.isEmpty()) {
		return;
	}
	const auto ids = ReadActionOrder(raw);
	auto ranked = std::vector<not_null<QAction*>>();
	for (const auto action : menu->actions()) {
		if (action->property(kActionIdProperty).isValid()) {
			ranked.push_back(action);
		}
	}
	const auto rank = [&](not_null<QAction*> action) {
		const auto id = ActionId(action->property(kActionIdProperty).toInt());
		return std::find(ids.begin(), ids.end(), id) - ids.begin();
	};
	std::stable_sort(ranked.begin(), ranked.end(), [&](const auto a, const auto b) {
		return rank(a) < rank(b);
	});
	auto order = menu->actions();
	auto index = 0;
	for (auto &action : order) {
		if (action->property(kActionIdProperty).isValid()) {
			action = ranked[index++];
		}
	}
	menu->menu()->reorderActions(order);
}

} // namespace

const style::PopupMenu &MessageMenuStyle() {
	return ForDevice().Get(kCompactMenu)
		? st::nagramCompactPopupMenu
		: st::popupMenuWithIcons;
}

void Tag(QAction *action, ActionId id) {
	Expects(action != nullptr);
	action->setProperty(kActionIdProperty, static_cast<int>(id));
}

int EndPosition(not_null<Ui::PopupMenu*> menu) {
	auto slots = std::vector<Slot>();
	slots.reserve(menu->actions().size());
	for (const auto &action : menu->actions()) {
		const auto value = action->property(kActionIdProperty);
		slots.push_back({
			.id = value.isValid()
				? std::make_optional(ActionId(value.toInt()))
				: std::nullopt,
			.separator = action->isSeparator(),
		});
	}
	return EndPosition(slots);
}

bool Shown(ActionId id) {
	const auto optionHeld = (QGuiApplication::keyboardModifiers()
		& Qt::AltModifier) != 0;
	return Visible(
		ReadVisibility(ForDevice().Get(kMenuConfig), id),
		optionHeld);
}

not_null<Ui::PopupMenu*> Destination(not_null<Ui::PopupMenu*> menu, ActionId id) {
	const auto storedRoot = menu->property(kRootProperty).value<QObject*>();
	const auto root = storedRoot
		? not_null<Ui::PopupMenu*>(static_cast<Ui::PopupMenu*>(storedRoot))
		: menu;
	const auto grouped = ReadActionList(ForDevice().Get(kRegramSubmenu));
	if (!Shown(id) || !ranges::contains(grouped, id)) {
		return root;
	}
	for (const auto action : root->actions()) {
		if (action->property(kSubmenuProperty).toBool()) {
			return root->ensureSubmenu(action, root->st());
		}
	}
	auto submenu = std::make_unique<Ui::PopupMenu>(root, root->st());
	const auto result = submenu.get();
	result->setProperty(kRootProperty, QVariant::fromValue<QObject*>(root));
	root->addAction(u"Regram"_q, std::move(submenu), &st::menuIconNagram)
		->setProperty(kSubmenuProperty, true);
	return result;
}

void Apply(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller,
		MessageIdsList selected,
		Fn<void(HistoryItem*)> select) {
	Expects(menu != nullptr);
	if (controller) {
		if (item) {
			InsertRepeatActions(menu, item, controller);
			InsertQuickRatingActions(menu, item, controller);
		}
		if (item || !selected.empty()) {
			InsertBatchActions(menu, item, controller,
				selected, select);
			Snapshot::InsertAction(menu, controller, item, selected);
			InsertSummaryAction(menu, item, controller, selected);
		}
		if (item) {
			InsertMediaInfoAction(menu, item, controller);
			InsertDeleteDownloadAction(menu, item, controller);
			InsertReadingAction(menu, item, controller);
			Filters::InsertAuthorAction(menu, item, controller);
			InsertMessageToolActions(menu, item, controller, select);
		}
	}
	for (auto index = int(menu->actions().size()); index != 0;) {
		--index;
		const auto value = menu->actions()[index]->property(kActionIdProperty);
		if (value.isValid() && !Shown(ActionId(value.toInt()))) {
			menu->removeAction(index);
		}
	}
	// Menu::removeAction leaves the following items with their old indexes,
	// and an item with a wrong index deselects itself when it gets hovered.
	for (auto index = 0; index != int(menu->actions().size()); ++index) {
		if (const auto widget = menu->menu()->itemForAction(
				menu->actions()[index])) {
			widget->setIndex(index);
		}
	}
	for (const auto action : menu->actions()) {
		if (action->property(kSubmenuProperty).toBool()) {
			Reorder(menu->ensureSubmenu(action, menu->st()));
		}
	}
	for (auto index = int(menu->actions().size()); index != 0;) {
		--index;
		const auto action = menu->actions()[index];
		if (action->property(kSubmenuProperty).toBool()
			&& menu->ensureSubmenu(action, menu->st())->empty()) {
			menu->removeAction(index);
		}
	}
	Reorder(menu);
	auto previousSeparator = true;
	for (auto index = 0; index != int(menu->actions().size());) {
		const auto separator = menu->actions()[index]->isSeparator();
		if (separator && previousSeparator) {
			menu->removeAction(index);
		} else {
			previousSeparator = separator;
			++index;
		}
	}
	if (!menu->actions().empty() && menu->actions().back()->isSeparator()) {
		menu->removeAction(int(menu->actions().size()) - 1);
	}
	for (auto index = 0; index != int(menu->actions().size()); ++index) {
		if (const auto widget = menu->menu()->itemForAction(menu->actions()[index])) {
			widget->setIndex(index);
		}
	}
}

} // namespace Nagram::Menu
