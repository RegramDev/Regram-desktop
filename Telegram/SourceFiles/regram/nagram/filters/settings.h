#pragma once

#include <gsl/pointers>

class PeerData;
namespace Main { class Session; }
namespace Ui { class GenericBox; }

namespace Nagram::Filters {

void SettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void GlobalSettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void ScopesSettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void ScopeSettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session,
	QString peer,
	QString topic);

} // namespace Nagram::Filters
