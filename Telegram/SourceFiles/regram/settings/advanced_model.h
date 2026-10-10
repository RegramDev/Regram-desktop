#pragma once

#include "regram/nagram/core/options.h"

#include <QtCore/QJsonObject>

namespace Regram::Settings {

[[nodiscard]] QByteArray PortableAdvancedSettings(const QJsonObject &source);
[[nodiscard]] std::optional<QJsonObject> ParseAdvancedSettings(const QByteArray &raw);
[[nodiscard]] bool ValidAdvancedSettings(const QByteArray &raw);

inline const auto kAdvancedSettings = Nagram::Option<QByteArray>{
	"nagram.regramAdvancedSettings", Nagram::Scope::Device, QByteArray(),
	Nagram::Category::Privacy, "lng_regram_advanced_backup",
	static_cast<unsigned>(Nagram::Flag::RequiresRestart)
		| static_cast<unsigned>(Nagram::Flag::Hidden)
		| static_cast<unsigned>(Nagram::Flag::Exportable)
		| static_cast<unsigned>(Nagram::Flag::LocalOnly), ValidAdvancedSettings };

} // namespace Regram::Settings
