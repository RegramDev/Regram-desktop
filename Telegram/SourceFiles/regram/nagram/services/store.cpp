#include "regram/nagram/services/model.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Nagram {
namespace {

QJsonObject ActiveServices(QJsonObject value) {
	auto instances = QJsonArray();
	for (const auto &entry : value.value(u"instances"_q).toArray()) {
		const auto service = ParseService(entry.toObject());
		if (service && service->kind == ServiceKind::Translation) {
			instances.push_back(entry);
		} else if (service) {
			for (const auto &key : { u"translation"_q, u"summary"_q }) {
				if (value.value(key).toString() == service->id) {
					value.insert(key, QString());
				}
			}
		}
	}
	value.insert(u"instances"_q, instances);
	value.insert(u"transcription"_q, QString());
	return value;
}

} // namespace

std::optional<QJsonObject> Services() {
	const auto bytes = ForDevice().Get(kServicesConfig);
	if (ForDevice().invalidKeys().contains(kServicesConfig.key)) {
		LOG(("Nagram Error: Invalid services configuration; external services disabled."));
		return std::nullopt;
	}
	if (bytes.isEmpty()) {
		return ServicesDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidServices(document.object())) {
		LOG(("Nagram Error: Invalid services configuration; external services disabled."));
		return std::nullopt;
	}
	return ActiveServices(UpgradeServices(document.object()));
}

bool SetServices(const QJsonObject &stored) {
	const auto upgraded = UpgradeServices(stored);
	if (!ValidServices(upgraded)) {
		return false;
	}
	const auto value = ActiveServices(upgraded);
	return ValidServices(value) && ForDevice().Set(kServicesConfig,
		value == ServicesDefaults() ? QByteArray()
			: QJsonDocument(value).toJson(QJsonDocument::Compact));
}

bool PrefersSystemAi() {
#ifdef Q_OS_MAC
	return ForDevice().Get(kPreferSystemAi);
#else // Q_OS_MAC
	return false; // an imported or synced value must not break the AI button
#endif // Q_OS_MAC
}

} // namespace Nagram
