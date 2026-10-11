#pragma once

class HistoryItem;

namespace Data {
class Thread;
} // namespace Data

namespace Regram::Filters {

[[nodiscard]] HistoryItem *PreviewItem(
	not_null<Data::Thread*> thread,
	not_null<HistoryItem*> top);

} // namespace Regram::Filters
