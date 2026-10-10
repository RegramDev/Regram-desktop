#include "regram/filters/search_text.h"

#include "history/history_item.h"

namespace Regram::Filters {

QString SearchText(not_null<HistoryItem*> item) {
	return item->originalText().text;
}

} // namespace Regram::Filters
