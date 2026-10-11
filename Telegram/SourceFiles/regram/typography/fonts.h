#pragma once

#include <QtCore/QJsonObject>
#include <optional>

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Regram::Typography {

[[nodiscard]] QJsonObject IosFontSettings();
[[nodiscard]] QJsonObject NativeFontSettings();
[[nodiscard]] std::optional<QJsonObject> ResolveIosFonts(const QJsonObject &settings);
void ApplyIosFonts(const QJsonObject &settings);

void Start();
void SettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

} // namespace Regram::Typography
