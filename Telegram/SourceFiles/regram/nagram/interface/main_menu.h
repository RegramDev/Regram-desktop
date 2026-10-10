#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <gsl/pointers>

#include <optional>
#include <array>

namespace Ui {
class GenericBox;
class VerticalLayout;
} // namespace Ui

namespace Nagram::Interface {

inline constexpr auto kMainMenuIds = std::array{
	"profile", "bots", "newGroup", "newChannel", "contacts", "calls",
	"savedMessages", "recentChats", "downloads", "settings", "nightMode",
};

[[nodiscard]] QString MainMenuActionTitle(const QString &id);
[[nodiscard]] QJsonObject MainMenuDefaults();
[[nodiscard]] bool ValidMainMenu(const QJsonObject &value);
[[nodiscard]] bool ValidMainMenuBytes(const QByteArray &value);
[[nodiscard]] std::optional<QJsonObject> MainMenu();
void SetMainMenu(const QJsonObject &value);
[[nodiscard]] QString MainMenuTitle();
[[nodiscard]] bool MainMenuSeasonal();
[[nodiscard]] bool MainMenuCustomOrder();
[[nodiscard]] gsl::not_null<Ui::VerticalLayout*> AddMainMenuGroup(
	gsl::not_null<Ui::VerticalLayout*> menu,
	const QString &id);
void MainMenuBox(gsl::not_null<Ui::GenericBox*> box);

} // namespace Nagram::Interface
