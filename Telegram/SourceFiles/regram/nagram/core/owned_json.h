#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace Nagram {

[[nodiscard]] inline std::optional<quint64> DecimalId(
		const QJsonValue &value,
		bool allowZero = false) {
	if (!value.isString()) {
		return std::nullopt;
	}
	const auto text = value.toString();
	auto ok = false;
	const auto id = text.toULongLong(&ok);
	return (ok && (id || allowZero) && QString::number(id) == text)
		? std::make_optional(quint64(id))
		: std::nullopt;
}

// A list left in the account prefs by another user must read as empty.
[[nodiscard]] inline std::optional<QJsonObject> ParseOwnedJson(
		const QByteArray &raw,
		QStringList keys) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	const auto object = document.object();
	keys.sort();
	return (object.keys() == keys
		&& object.value(QStringLiteral("version")).isDouble()
		&& object.value(QStringLiteral("version")).toDouble() == 1.
		&& DecimalId(object.value(QStringLiteral("user"))))
		? std::make_optional(object)
		: std::nullopt;
}

[[nodiscard]] inline bool OwnedBy(const QJsonObject &object, quint64 user) {
	return DecimalId(object.value(QStringLiteral("user"))) == user;
}

[[nodiscard]] inline QJsonObject NewOwnedJson(quint64 user) {
	return QJsonObject{
		{ QStringLiteral("version"), 1 },
		{ QStringLiteral("user"), QString::number(user) },
	};
}

} // namespace Nagram
