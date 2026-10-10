#pragma once

#include "ui/text/text_entity.h"

class HistoryItem;

namespace Nagram::Filters {

struct Result {
	TextWithEntities text;
	bool hidden = false;
};

[[nodiscard]] Result Project(
	HistoryItem *item,
	const TextWithEntities &source,
	bool displayOverrides = true);
[[nodiscard]] bool Hidden(HistoryItem *item);
[[nodiscard]] bool HiddenForNotification(HistoryItem *item);
[[nodiscard]] TextWithEntities DisplayText(const Result &result);

} // namespace Nagram::Filters
