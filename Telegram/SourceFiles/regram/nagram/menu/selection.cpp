#include "regram/nagram/menu/selection.h"

#include "history/history_inner_widget.h"
#include "history/history_widget.h"
#include "history/view/history_view_list_widget.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"

namespace Nagram::Menu {
namespace {

template <typename Selected>
std::optional<std::pair<int, int>> RangeBounds(
		const std::vector<not_null<HistoryItem*>> &views,
		Selected &&selected) {
	auto first = -1;
	auto last = -1;
	for (auto index = 0; index != int(views.size()); ++index) {
		if (selected(views[index])) {
			if (first < 0) {
				first = index;
			}
			last = index;
		}
	}
	return (first >= 0 && last > first)
		? std::make_optional(std::make_pair(first, last))
		: std::nullopt;
}

} // namespace

void Selection::Select(HistoryInner *widget, HistoryItem *source) {
	if (!widget || widget->hasSelectRestriction()) {
		return;
	}
	auto selected = widget->_selected;
	auto views = std::vector<not_null<HistoryItem*>>();
	for (const auto &history : { widget->_migrated, widget->_history.get() }) {
		if (history) {
			for (const auto &block : history->blocks) {
				for (const auto &view : block->messages) {
					views.push_back(view->data());
				}
			}
		}
	}
	const auto range = RangeBounds(views, [&](not_null<HistoryItem*> item) {
		return selected.contains(item);
	}).value_or(std::make_pair(0, int(views.size()) - 1));
	for (auto index = 0; index != int(views.size()); ++index) {
		const auto item = views[index];
		if (!item->canBeSelected() || (source
				? (item->from() != source->from())
				: (index < range.first || index > range.second))) {
			continue;
		}
		if (!selected.contains(item) && selected.size() >= MaxSelectedItems) {
			widget->_controller->showToast(
				tr::lng_nagram_menu_selection_limit(tr::now));
			break;
		}
		widget->changeSelection(&selected, item,
			HistoryInner::SelectAction::Select);
	}
	widget->clearTextSelection();
	widget->_selected = std::move(selected);
	widget->_accessibilitySelectionAnchor = nullptr;
	widget->update();
	widget->_widget->updateTopBarSelection();
}

void Selection::Select(HistoryView::ListWidget *widget, HistoryItem *source) {
	if (!widget || widget->hasSelectRestriction()) {
		return;
	}
	auto selected = widget->_selected;
	auto views = std::vector<not_null<HistoryItem*>>();
	for (const auto &view : widget->_items) {
		views.push_back(view->data());
	}
	const auto range = RangeBounds(views, [&](not_null<HistoryItem*> item) {
		return selected.contains(item->fullId());
	}).value_or(std::make_pair(0, int(views.size()) - 1));
	for (auto index = 0; index != int(views.size()); ++index) {
		const auto item = views[index];
		if (!widget->_delegate->listIsItemGoodForSelection(item) || (source
				? (item->from() != source->from())
				: (index < range.first || index > range.second))) {
			continue;
		}
		if (!selected.contains(item->fullId())
			&& selected.size() >= MaxSelectedItems) {
			widget->controller()->showToast(
				tr::lng_nagram_menu_selection_limit(tr::now));
			break;
		}
		widget->changeSelection(selected, item,
			HistoryView::ListWidget::SelectAction::Select);
	}
	widget->clearTextSelection();
	widget->_selected = std::move(selected);
	widget->_accessibilitySelectionAnchor = nullptr;
	widget->pushSelectedItems();
	widget->update();
}

} // namespace Nagram::Menu
