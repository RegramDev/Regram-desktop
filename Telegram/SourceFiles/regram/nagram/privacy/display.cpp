#include "regram/nagram/privacy/display.h"

#include "regram/nagram/privacy/options.h"
#include "boxes/moderate_messages_box.h"

#include <QtCore/QCalendar>
#include <QtCore/QLocale>

namespace Nagram::Privacy {

std::optional<bool> LastNameFirst() {
	// Names are composed once per peer, so the choice is read at startup.
	static const auto order = ForDevice().Get(kNameOrder);
	return order ? std::make_optional(order == 2) : std::nullopt;
}

std::optional<QString> PersianDate(const QDate &date, DateStyle style) {
#if QT_CONFIG(jalalicalendar)
	const auto mode = ForDevice().Get(kPersianCalendar);
	if (!mode || !date.isValid()) {
		return std::nullopt;
	}
	const auto calendar = QCalendar(QCalendar::System::Jalali);
	const auto parts = calendar.partsFromDate(date);
	const auto today = calendar.partsFromDate(QDate::currentDate());
	if (!parts.isValid() || !today.isValid()) {
		return std::nullopt;
	}
	const auto locale = (mode == 2)
		? QLocale(QLocale::English)
		: QLocale(QLocale::Persian);
	const auto full = (style == DateStyle::DayFull)
		|| (style == DateStyle::MonthFull);
	const auto month = calendar.monthName(
		locale,
		parts.month,
		parts.year,
		full ? QLocale::LongFormat : QLocale::ShortFormat);
	auto result = (style == DateStyle::Day || style == DateStyle::DayFull)
		? (QString::number(parts.day) + u' ' + month)
		: month;
	if (parts.year != today.year) {
		result += u' ' + QString::number(parts.year);
	}
	return result;
#else // QT_CONFIG(jalalicalendar)
	return std::nullopt;
#endif // QT_CONFIG(jalalicalendar)
}

void ApplyModerateDefaults(ModerateMessagesBoxOptions &options) {
	const auto defaults = ForDevice().Get(kModerateDefaults);
	options.reportSpam |= (defaults & 1) != 0;
	options.deleteAll |= (defaults & 2) != 0;
	options.banUser |= (defaults & 4) != 0;
}

} // namespace Nagram::Privacy
