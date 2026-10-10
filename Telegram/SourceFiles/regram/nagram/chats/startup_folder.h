#pragma once

#include "data/data_types.h"

namespace Main { class Session; }

namespace Nagram::Chats {

[[nodiscard]] FilterId StartupFolder(
	gsl::not_null<Main::Session*> session,
	FilterId defaultId);
void VerifyStartupFolder(gsl::not_null<Main::Session*> session);
void RememberFolder(gsl::not_null<Main::Session*> session, FilterId id);

} // namespace Nagram::Chats
