#include "regram/nagram/snapshot/cloud_theme_model.h"

#include "regram/nagram/core/owned_json.h"

namespace Nagram::Snapshot {
namespace {

constexpr auto kSlugLimit = 64;

[[nodiscard]] QStringList Keys() {
	return {
		QStringLiteral("version"),
		QStringLiteral("user"),
		QStringLiteral("themeId"),
		QStringLiteral("accessHash"),
		QStringLiteral("documentId"),
		QStringLiteral("title"),
		QStringLiteral("slug"),
	};
}

[[nodiscard]] bool SingleLine(const QString &text, int limit) {
	return text.size() <= limit
		&& !text.contains(u'\n')
		&& !text.contains(u'\r');
}

} // namespace

std::optional<CloudThemeRef> ParseCloudThemeRef(const QByteArray &raw) {
	const auto object = ParseOwnedJson(raw, Keys());
	if (!object) {
		return std::nullopt;
	}
	const auto user = DecimalId(object->value(QStringLiteral("user")));
	const auto theme = DecimalId(object->value(QStringLiteral("themeId")));
	const auto access = DecimalId(
		object->value(QStringLiteral("accessHash")));
	const auto document = DecimalId(
		object->value(QStringLiteral("documentId")));
	const auto title = object->value(QStringLiteral("title"));
	const auto slug = object->value(QStringLiteral("slug"));
	if (!user || !theme || !access || !document
		|| !title.isString()
		|| !slug.isString()
		|| !SingleLine(title.toString(), kCloudThemeTitleLimit)
		|| !SingleLine(slug.toString(), kSlugLimit)) {
		return std::nullopt;
	}
	return CloudThemeRef{
		.user = *user,
		.themeId = *theme,
		.accessHash = *access,
		.documentId = *document,
		.title = title.toString(),
		.slug = slug.toString(),
	};
}

QByteArray SerializeCloudThemeRef(const CloudThemeRef &ref) {
	auto object = NewOwnedJson(ref.user);
	object.insert(QStringLiteral("themeId"), QString::number(ref.themeId));
	object.insert(
		QStringLiteral("accessHash"),
		QString::number(ref.accessHash));
	object.insert(
		QStringLiteral("documentId"),
		QString::number(ref.documentId));
	object.insert(
		QStringLiteral("title"),
		ref.title.simplified().left(kCloudThemeTitleLimit));
	object.insert(QStringLiteral("slug"), ref.slug.simplified().left(kSlugLimit));
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool ValidCloudThemeRef(const QByteArray &raw) {
	return raw.isEmpty() || ParseCloudThemeRef(raw).has_value();
}

bool ValidCloudAccount(const QString &value) {
	if (value.isEmpty()) {
		return true;
	}
	auto ok = false;
	const auto id = value.toULongLong(&ok);
	return ok && id && (QString::number(id) == value);
}

} // namespace Nagram::Snapshot
