#include "regram/nagram/services/model.h"

#include <QtCore/QJsonDocument>

namespace Nagram {

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
	return UpgradeServices(document.object());
}

bool SetServices(const QJsonObject &stored) {
	const auto value = UpgradeServices(stored);
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
