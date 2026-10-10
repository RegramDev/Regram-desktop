#pragma once

class HistoryInner;
class HistoryItem;
namespace HistoryView {
class ListWidget;
} // namespace HistoryView

namespace Nagram::Menu {

class Selection final {
public:
	static void Select(HistoryInner *widget, HistoryItem *source);
	static void Select(HistoryView::ListWidget *widget, HistoryItem *source);
};

} // namespace Nagram::Menu
