#include "regram/nagram/media/local_faved_model.h"

#include "regram/nagram/core/owned_json.h"

#include <QtCore/QJsonArray>

#include <algorithm>

namespace Nagram::Media {
namespace {

[[nodiscard]] std::optional<QJsonObject> ParseObject(const QByteArray &raw) {
	const auto object = ParseOwnedJson(raw, {
		QStringLiteral("version"),
		QStringLiteral("user"),
		QStringLiteral("items"),
	});
	const auto items = object
		? object->value(QStringLiteral("items"))
		: QJsonValue();
	return (items.isArray() && items.toArray().size() <= kLocalFavedLimit)
		? object
		: std::nullopt;
}

[[nodiscard]] std::optional<LocalFavedItem> ParseItem(
		const QJsonValue &value) {
	const auto object = value.toObject();
	const auto id = DecimalId(object.value(QStringLiteral("id")));
	const auto set = DecimalId(object.value(QStringLiteral("set")));
	const auto hash = DecimalId(object.value(QStringLiteral("hash")), true);
	const auto app = object.value(QStringLiteral("app"));
	const auto data = object.value(QStringLiteral("data"));
	if (object.size() != 5
		|| !id
		|| !set
		|| !hash
		|| !app.isDouble()
		|| app.toInt() <= 0
		|| app.toDouble() != double(app.toInt())
		|| !data.isString()) {
		return std::nullopt;
	}
	const auto encoded = data.toString().toLatin1();
	auto decoded = QByteArray::fromBase64Encoding(
		encoded,
		QByteArray::AbortOnBase64DecodingErrors);
	if (!decoded || (*decoded).isEmpty()) {
		return std::nullopt;
	}
	return LocalFavedItem{ *id, *set, *hash, app.toInt(), *decoded };
}

[[nodiscard]] auto Find(std::vector<LocalFavedItem> &items, quint64 id) {
	return std::find_if(items.begin(), items.end(), [&](const auto &item) {
		return item.id == id;
	});
}

} // namespace

bool ValidLocalFaved(const QByteArray &raw) {
	return ParseObject(raw).has_value();
}

std::vector<LocalFavedItem> ParseLocalFaved(
		const QByteArray &raw,
		quint64 user,
		int *skipped) {
	auto result = std::vector<LocalFavedItem>();
	const auto object = ParseObject(raw);
	if (!object || !OwnedBy(*object, user)) {
		return result;
	}
	for (const auto &value : object->value(QStringLiteral("items")).toArray()) {
		auto item = ParseItem(value);
		if (item && Find(result, item->id) == result.end()) {
			result.push_back(std::move(*item));
		} else if (skipped) {
			++*skipped;
		}
	}
	return result;
}

QByteArray SerializeLocalFaved(
		const std::vector<LocalFavedItem> &items,
		quint64 user) {
	if (items.empty()) {
		return QByteArray();
	}
	auto list = QJsonArray();
	for (const auto &item : items) {
		list.push_back(QJsonObject{
			{ QStringLiteral("id"), QString::number(item.id) },
			{ QStringLiteral("set"), QString::number(item.set) },
			{ QStringLiteral("hash"), QString::number(item.hash) },
			{ QStringLiteral("app"), item.app },
			{ QStringLiteral("data"), QString::fromLatin1(item.data.toBase64()) },
		});
	}
	auto object = NewOwnedJson(user);
	object.insert(QStringLiteral("items"), list);
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool AddFavedItem(std::vector<LocalFavedItem> &items, LocalFavedItem item) {
	if (!item.id || !item.set || item.app <= 0 || item.data.isEmpty()) {
		return false;
	}
	const auto i = Find(items, item.id);
	if (i != items.end()) {
		*i = std::move(item);
		std::rotate(items.begin(), i, i + 1);
		return true;
	} else if (int(items.size()) >= kLocalFavedLimit) {
		return false;
	}
	items.insert(items.begin(), std::move(item));
	return true;
}

bool RemoveFavedItem(std::vector<LocalFavedItem> &items, quint64 id) {
	const auto i = Find(items, id);
	if (i == items.end()) {
		return false;
	}
	items.erase(i);
	return true;
}

bool MergeFavedItems(
		std::vector<LocalFavedItem> &items,
		const std::vector<quint64> &serverIds) {
	const auto was = items.size();
	std::erase_if(items, [&](const LocalFavedItem &item) {
		return std::find(serverIds.begin(), serverIds.end(), item.id)
			!= serverIds.end();
	});
	return items.size() != was;
}

} // namespace Nagram::Media
