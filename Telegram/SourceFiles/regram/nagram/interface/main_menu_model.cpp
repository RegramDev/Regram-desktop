#include "regram/nagram/interface/main_menu.h"
#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>

#include <algorithm>

namespace Nagram::Interface {

QJsonObject MainMenuDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"order"_q, QJsonArray() },
		{ u"hidden"_q, QJsonArray() },
		{ u"title"_q, QString() },
		{ u"seasonalDecorations"_q, true },
	};
}

bool ValidMainMenu(const QJsonObject &value) {
	if (value.keys() != MainMenuDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"title"_q).isString()
		|| !value.value(u"seasonalDecorations"_q).isBool()) {
		return false;
	}
	const auto title = value.value(u"title"_q).toString();
	if (title.size() > 96 || QString::fromUtf8(title.toUtf8()) != title
		|| std::any_of(title.begin(), title.end(), [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		})) {
		return false;
	}
	for (const auto &key : { u"order"_q, u"hidden"_q }) {
		if (!value.value(key).isArray()) {
			return false;
		}
		auto seen = QSet<QString>();
		for (const auto &item : value.value(key).toArray()) {
			const auto id = item.toString();
			if (!item.isString() || seen.contains(id)
				|| (key == u"hidden"_q && id == u"settings"_q)
				|| !std::any_of(kMainMenuIds.begin(), kMainMenuIds.end(),
					[&](const char *known) { return id == QLatin1String(known); })) {
				return false;
			}
			seen.insert(id);
		}
	}
	return true;
}

bool ValidMainMenuBytes(const QByteArray &value) {
	if (value.isEmpty()) {
		return true;
	}
	const auto document = QJsonDocument::fromJson(value);
	return document.isObject() && ValidMainMenu(document.object());
}

} // namespace Nagram::Interface
