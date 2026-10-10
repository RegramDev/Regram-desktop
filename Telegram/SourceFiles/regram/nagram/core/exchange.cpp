#include "regram/nagram/core/exchange.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram {
namespace {

constexpr auto kVersion = 1;
constexpr auto kMaxImportBytes = 8 * 1024 * 1024;

std::optional<QJsonValue> Decode(
		const OptionInfo &info,
		const QByteArray &raw) {
	if (raw.isEmpty()) {
		return std::nullopt;
	}
	auto value = QJsonValue();
	switch (info.type) {
	case OptionInfo::ValueType::Boolean:
		if (raw != "1" && raw != "0") {
			return std::nullopt;
		}
		value = (raw == "1");
		break;
	case OptionInfo::ValueType::Integer: {
		auto ok = false;
		const auto number = raw.toInt(&ok);
		if (!ok || QByteArray::number(number) != raw) {
			return std::nullopt;
		}
		value = number;
		break;
	}
	case OptionInfo::ValueType::String: {
		if (!raw.startsWith('s')) {
			return std::nullopt;
		}
		const auto bytes = raw.mid(1);
		const auto decoded = QString::fromUtf8(bytes);
		if (decoded.toUtf8() != bytes) {
			return std::nullopt;
		}
		value = decoded;
		break;
	}
	case OptionInfo::ValueType::Object: {
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(raw, &error);
		if (error.error != QJsonParseError::NoError || !document.isObject()) {
			return std::nullopt;
		}
		value = document.object();
		break;
	}
	}
	return info.accepts(value) ? std::optional<QJsonValue>(value) : std::nullopt;
}

QByteArray Encode(const OptionInfo &info, const QJsonValue &value) {
	auto raw = QByteArray();
	switch (info.type) {
	case OptionInfo::ValueType::Boolean:
		raw = value.toBool() ? "1" : "0";
		break;
	case OptionInfo::ValueType::Integer:
		raw = QByteArray::number(value.toInt());
		break;
	case OptionInfo::ValueType::String:
		raw = "s" + value.toString().toUtf8();
		break;
	case OptionInfo::ValueType::Object:
		raw = QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
		break;
	}
	return raw == info.fallbackRaw ? QByteArray() : raw;
}

bool Exportable(const OptionInfo &info) {
	return info.scope == Scope::Device
		&& (info.flags & static_cast<unsigned>(Flag::Exportable)) != 0;
}

bool Carried(const OptionInfo &info, ExchangeTarget target) {
	return Exportable(info)
		&& (target == ExchangeTarget::File
			|| !(info.flags & static_cast<unsigned>(Flag::LocalOnly)));
}

} // namespace

ExchangeExport Exchange::Export(
		Options &options,
		const Registry &registry,
		ExchangeTarget target) {
	auto result = ExchangeExport();
	if (options._scope != Scope::Device) {
		return result;
	}
	auto values = QJsonObject();
	for (const auto &info : registry.All()) {
		if (!Carried(info, target)) {
			continue;
		}
		const auto raw = options._prefs.read(info.key);
		if (raw.isEmpty()) {
			continue;
		}
		const auto value = Decode(info, raw);
		const auto key = QString::fromUtf8(info.key.data(), info.key.size());
		if (value) {
			values.insert(key, *value);
		} else {
			result.invalidKeys.push_back(key);
		}
	}
	auto root = QJsonObject();
	root.insert(u"version", kVersion);
	root.insert(u"options", values);
	result.data = QJsonDocument(root).toJson(QJsonDocument::Indented);
	return result;
}

ExchangePlan Exchange::PlanImport(
		Options &options,
		const Registry &registry,
		const QByteArray &data,
		ExchangeTarget target) {
	auto result = ExchangePlan();
	if (options._scope != Scope::Device) {
		result.error = QString::fromLatin1("Only device settings can be imported.");
		return result;
	}
	if (data.size() > kMaxImportBytes) {
		result.error = QString::fromLatin1("The settings file is too large.");
		return result;
	}
	auto parseError = QJsonParseError();
	const auto document = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		result.error = QString::fromLatin1("The settings file is not valid JSON.");
		return result;
	}
	const auto root = document.object();
	const auto version = root.value("version");
	if (root.size() != 2 || !version.isDouble()
		|| version.toDouble() != kVersion
		|| !root.value("options").isObject()) {
		result.error = QString::fromLatin1("The settings file has an unsupported schema.");
		return result;
	}
	const auto values = root.value("options").toObject();
	for (auto it = values.begin(); it != values.end(); ++it) {
		const auto encodedKey = it.key().toUtf8();
		const auto info = registry.Find(std::string_view(
			encodedKey.constData(), encodedKey.size()));
		if (!info || !Carried(*info, target)) {
			result.skippedKeys.push_back(it.key());
			continue;
		}
		if (!info->accepts(it.value())) {
			result.error = QString::fromLatin1("Invalid setting value: ") + it.key();
			result.changes.clear();
			return result;
		}
		const auto value = info->imported
			? info->imported(it.value())
			: it.value();
		if (value != it.value()) {
			result.adjustedKeys.push_back(it.key());
		}
		const auto after = Encode(*info, value);
		const auto before = options._prefs.read(info->key);
		if (before != after) {
			result.changes.push_back({ it.key(), before, after });
		}
	}
	return result;
}

ExchangeApply Exchange::Apply(
		Options &options,
		const Registry &registry,
		const ExchangePlan &plan) {
	if (options._scope != Scope::Device || !plan.error.isEmpty()) {
		return { false, QString::fromLatin1("The import plan is invalid.") };
	}
	for (const auto &change : plan.changes) {
		const auto encodedKey = change.key.toUtf8();
		const auto info = registry.Find(std::string_view(
			encodedKey.constData(), encodedKey.size()));
		if (!info || !Exportable(*info)
			|| options._prefs.read(info->key) != change.before
			|| (!change.after.isEmpty() && !Decode(*info, change.after))) {
			return { false, QString::fromLatin1("The settings changed since preview: ")
				+ change.key };
		}
	}
	for (const auto &change : plan.changes) {
		const auto encodedKey = change.key.toUtf8();
		const auto info = registry.Find(std::string_view(
			encodedKey.constData(), encodedKey.size()));
		if (change.after.isEmpty()) {
			options._prefs.clear(info->key);
		} else {
			options._prefs.write(info->key, change.after);
		}
		options._invalidKeys.erase(info->key);
	}
	for (const auto &change : plan.changes) {
		const auto encodedKey = change.key.toUtf8();
		const auto info = registry.Find(std::string_view(
			encodedKey.constData(), encodedKey.size()));
		options._changes.fire_copy(info->key);
	}
	return { true, {} };
}

} // namespace Nagram
