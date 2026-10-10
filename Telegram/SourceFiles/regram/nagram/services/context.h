#pragma once

#include <QtCore/QStringList>

class HistoryItem;

namespace Nagram {

[[nodiscard]] QStringList CollectTranslationContext(
	not_null<HistoryItem*> item);

} // namespace Nagram
