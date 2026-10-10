#pragma once

#include <QtCore/QDate>
#include <QtCore/QString>

#include <optional>

struct ModerateMessagesBoxOptions;

namespace Nagram::Privacy {

enum class DateStyle {
	Day,
	DayFull,
	Month,
	MonthFull,
};

[[nodiscard]] std::optional<bool> LastNameFirst();
[[nodiscard]] std::optional<QString> PersianDate(
	const QDate &date,
	DateStyle style);
void ApplyModerateDefaults(ModerateMessagesBoxOptions &options);

} // namespace Nagram::Privacy
