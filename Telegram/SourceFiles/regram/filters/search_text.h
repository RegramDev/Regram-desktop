#pragma once

#include <QtCore/QString>

class HistoryItem;

namespace Regram::Filters {

[[nodiscard]] QString SearchText(not_null<HistoryItem*> item);

} // namespace Regram::Filters
