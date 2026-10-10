#pragma once

#include "regram/nagram/filters/model.h"

class HistoryItem;

namespace Nagram::Filters {

[[nodiscard]] Result Project(
	HistoryItem *item,
	const TextWithEntities &source,
	bool displayOverrides = true);
[[nodiscard]] bool Hidden(HistoryItem *item);
[[nodiscard]] bool HiddenForNotification(HistoryItem *item);
[[nodiscard]] TextWithEntities DisplayText(const Result &result);

} // namespace Nagram::Filters
