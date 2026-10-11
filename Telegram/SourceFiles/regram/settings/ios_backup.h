#pragma once

#include "regram/nagram/core/options.h"
#include "regram/settings/ios_backup_model.h"

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Regram::Settings::IosBackup {

inline const auto kSource = Nagram::Option<QByteArray>{
	"nagram.regramIosBackupSource", Nagram::Scope::Account, QByteArray(),
	Nagram::Category::Interface, "lng_regram_cross_platform_backup",
	static_cast<unsigned>(Nagram::Flag::Hidden), ValidateSource };

void ExportBox(not_null<Ui::GenericBox*> box, not_null<Window::SessionController*> controller);
void ShowImport(not_null<Window::SessionController*> controller, QByteArray bytes);

} // namespace Regram::Settings::IosBackup
