#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <set>
#include <optional>

namespace Regram::Settings::IosBackup {

enum class Category { Appearance, Messages, Privacy, Translation, Media, Notifications };
inline constexpr auto kMaximumBytes = 2 * 1024 * 1024;
inline const auto kAllCategories = std::set{
	Category::Appearance, Category::Messages, Category::Privacy,
	Category::Translation, Category::Media, Category::Notifications,
};

struct Document {
	QJsonObject values;
	QString createdAt;
	QString error;
};

[[nodiscard]] std::optional<Category> CategoryFor(const QString &key);
[[nodiscard]] Document Parse(const QByteArray &bytes);
[[nodiscard]] QByteArray Encode(const QJsonObject &values);
[[nodiscard]] QJsonObject Select(const QJsonObject &values, const std::set<Category> &categories);
[[nodiscard]] bool ValidateSource(const QByteArray &bytes);

} // namespace Regram::Settings::IosBackup
