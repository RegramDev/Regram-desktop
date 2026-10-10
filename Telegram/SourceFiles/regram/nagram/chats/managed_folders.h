#pragma once

#include "data/data_types.h"

class History;
namespace Main { class Session; }
namespace Ui::Menu { struct MenuCallback; }

namespace Nagram::Chats {

[[nodiscard]] bool AllowedInFolder(
	gsl::not_null<History*> history,
	FilterId folderId);
void AddManagedOnlyAction(
	const Ui::Menu::MenuCallback &addAction,
	gsl::not_null<Main::Session*> session,
	FilterId folderId);

} // namespace Nagram::Chats
