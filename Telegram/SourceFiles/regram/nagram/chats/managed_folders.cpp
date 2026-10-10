#include "regram/nagram/chats/managed_folders.h"

#include "regram/nagram/chats/options.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/widgets/menu/menu_add_action_callback.h"

#include <set>

#include "styles/style_media_player.h"

namespace Nagram::Chats {
namespace {

std::set<int> SelectedFolders(gsl::not_null<Main::Session*> session) {
	auto result = std::set<int>();
	const auto value = ForAccount(session).Get(kManagedFolderIds);
	for (const auto &part : value.split(u',', Qt::SkipEmptyParts)) {
		result.insert(part.toInt());
	}
	return result;
}

void RefreshFolder(gsl::not_null<Main::Session*> session) {
	const auto data = &session->data();
	const auto refresh = [=](not_null<PeerData*> peer) {
		if (const auto history = data->historyLoaded(peer->id)) {
			data->chatsFilters().refreshHistory(history);
		}
	};
	data->enumerateUsers(refresh);
	data->enumerateGroups(refresh);
	data->enumerateBroadcasts(refresh);
}

} // namespace

bool AllowedInFolder(
		gsl::not_null<History*> history,
		FilterId folderId) {
	if (!folderId
		|| !SelectedFolders(&history->session()).contains(folderId)) {
		return true;
	}
	const auto peer = history->peer;
	if (const auto chat = peer->asChat()) {
		return chat->amCreator() || chat->adminRights() != 0;
	} else if (const auto channel = peer->asChannel()) {
		return channel->amCreator() || channel->adminRights() != 0;
	}
	return false;
}

void AddManagedOnlyAction(
		const Ui::Menu::MenuCallback &addAction,
		gsl::not_null<Main::Session*> session,
		FilterId folderId) {
	if (!folderId) return;
	const auto selected = SelectedFolders(session).contains(folderId);
	addAction(tr::lng_nagram_managed_only(tr::now), [=] {
		auto ids = SelectedFolders(session);
		if (!ids.erase(folderId)) ids.insert(folderId);
		auto parts = QStringList();
		for (const auto &id : ids) parts.push_back(QString::number(id));
		Expects(ForAccount(session).Set(kManagedFolderIds, parts.join(u","_q)));
		RefreshFolder(session);
	}, selected ? &st::mediaPlayerMenuCheck : nullptr);
}

} // namespace Nagram::Chats
