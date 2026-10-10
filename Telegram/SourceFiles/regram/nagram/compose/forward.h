#pragma once

#include "base/flat_map.h"
#include "data/data_types.h"

struct HistoryItemCommonFields;

namespace Nagram::Compose {

// WHY: upstream shows a forward at once only for one message that keeps
// its author, so an album or a hidden author waited for the server.
class LocalForwards final {
public:
	explicit LocalForwards(const Data::ResolvedForwardDraft &draft);

	[[nodiscard]] bool possible() const;
	[[nodiscard]] HistoryItemCommonFields fields(
		HistoryItemCommonFields fields,
		not_null<HistoryItem*> item);

private:
	[[nodiscard]] uint64 groupedId(not_null<HistoryItem*> item);

	const Data::ResolvedForwardDraft &_draft;
	base::flat_map<MessageGroupId, uint64> _groups;

};

} // namespace Nagram::Compose
