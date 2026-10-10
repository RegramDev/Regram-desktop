#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Regram::Filters {

void SettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);

} // namespace Regram::Filters
