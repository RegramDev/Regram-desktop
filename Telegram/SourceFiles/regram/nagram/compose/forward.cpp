#include "regram/nagram/compose/forward.h"

#include "base/random.h"
#include "history/history_item.h"

namespace Nagram::Compose {

LocalForwards::LocalForwards(const Data::ResolvedForwardDraft &draft)
: _draft(draft) {
}

bool LocalForwards::possible() const {
	return !ranges::any_of(_draft.items, &HistoryItem::isEphemeral);
}

HistoryItemCommonFields LocalForwards::fields(
		HistoryItemCommonFields fields,
		not_null<HistoryItem*> item) {
	using Options = Data::ForwardOptions;
	fields.groupedId = groupedId(item);
	fields.ignoreForwardFrom = (_draft.options != Options::PreserveInfo);
	fields.ignoreForwardCaptions
		= (_draft.options == Options::NoNamesAndCaptions);
	return fields;
}

uint64 LocalForwards::groupedId(not_null<HistoryItem*> item) {
	const auto groupId = item->groupId();
	const auto parts = groupId
		? ranges::count(_draft.items, groupId, &HistoryItem::groupId)
		: 0;
	if (parts < 2) {
		return 0;
	}
	const auto i = _groups.find(groupId);
	return (i != end(_groups))
		? i->second
		: _groups.emplace(
			groupId,
			base::RandomValue<uint64>()).first->second;
}

} // namespace Nagram::Compose
