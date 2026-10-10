#include "regram/nagram/filters/view.h"

#include "regram/filters/runtime.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"

namespace Nagram::Filters {

Result Project(
		HistoryItem *item,
		const TextWithEntities &source,
		bool displayOverrides) {
	return {
		.text = source,
		.hidden = item && Regram::Filters::Hidden(item, displayOverrides),
	};
}

bool Hidden(HistoryItem *item) {
	return item && Regram::Filters::Hidden(item);
}

bool HiddenForNotification(HistoryItem *item) {
	return item && Regram::Filters::Hidden(item, false);
}

TextWithEntities DisplayText(const Result &result) {
	return result.hidden
		? TextWithEntities{ tr::lng_nagram_filter_hidden(tr::now) }
		: result.text;
}

} // namespace Nagram::Filters
