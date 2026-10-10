#include "regram/nagram/chats/startup_folder.h"

#include "regram/nagram/chats/options.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "main/main_session.h"

#include <algorithm>

namespace Nagram::Chats {
namespace {

bool HasFolder(gsl::not_null<Main::Session*> session, FilterId id) {
	if (!id) {
		return true;
	}
	const auto &list = session->data().chatsFilters().list();
	return std::any_of(list.begin(), list.end(), [=](const auto &filter) {
		return filter.id() == id;
	});
}

} // namespace

FilterId StartupFolder(
		gsl::not_null<Main::Session*> session,
		FilterId defaultId) {
	auto &options = ForAccount(session);
	const auto mode = options.Get(kStartupFolderMode);
	const auto id = mode == 1
		? options.Get(kLastOpenedFolderId)
		: mode == 2 ? options.Get(kStartupFolderId) : defaultId;
	if (HasFolder(session, id)) {
		return id;
	}
	if (mode == 2) {
		Expects(options.Set(kStartupFolderMode, 0));
	}
	return defaultId;
}

void VerifyStartupFolder(gsl::not_null<Main::Session*> session) {
	auto &options = ForAccount(session);
	if (options.Get(kStartupFolderMode) == 2
		&& !HasFolder(session, options.Get(kStartupFolderId))) {
		Expects(options.Set(kStartupFolderMode, 0));
	}
}

void RememberFolder(gsl::not_null<Main::Session*> session, FilterId id) {
	Expects(ForAccount(session).Set(kLastOpenedFolderId, int(id)));
}

} // namespace Nagram::Chats
