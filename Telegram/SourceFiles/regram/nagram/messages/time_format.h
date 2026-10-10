#pragma once

#include <QtCore/QLocale>
#include <QtCore/QTime>

namespace Nagram::Messages {

[[nodiscard]] inline QString FormatTime(QTime time, bool seconds) {
	const auto locale = QLocale();
	if (!seconds) {
		return locale.toString(time, QLocale::ShortFormat);
	}
	auto format = locale.timeFormat(QLocale::LongFormat);
	auto quoted = false;
	for (auto i = 0; i < format.size();) {
		if (format[i] == '\'') {
			quoted = !quoted;
		}
		if (!quoted && format[i] == 't') {
			format.remove(i, 1);
		} else {
			++i;
		}
	}
	return locale.toString(time, format.trimmed());
}

} // namespace Nagram::Messages
