#include "regram/nagram/chats/recent_chats.h"

#include "base/weak_ptr.h"
#include "boxes/peer_list_box.h"
#include "data/data_peer.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "history/history.h"
#include "dialogs/dialogs_key.h"
#include "main/main_session.h"
#include "ui/painter.h"
#include "styles/style_nagram_compose.h"
#include "window/window_session_controller.h"

namespace Nagram::Chats {
namespace {

std::vector<PeerId> ReadRecent(not_null<Main::Session*> session) {
	auto result = std::vector<PeerId>();
	const auto value = ForAccount(session).Get(kRecentChatsList);
	for (const auto &part : value.split(u',', Qt::SkipEmptyParts)) {
		result.push_back(PeerId(part.toULongLong()));
	}
	return result;
}

std::vector<FilterId> ReadRecentFolders(not_null<Main::Session*> session) {
	auto result = std::vector<FilterId>();
	const auto value = ForAccount(session).Get(kRecentFolderIds);
	for (const auto &part : value.split(u',', Qt::SkipEmptyParts)) {
		result.push_back(part.toInt());
	}
	return result;
}

// ChatFilter::contains runs per chat per folder, so it must not parse settings.
struct FolderCache {
	base::weak_ptr<Main::Session> session;
	base::flat_set<PeerId> peers;
	base::flat_set<FilterId> folders;
};
std::vector<FolderCache> Caches;

void ResetCache(not_null<Main::Session*> session) {
	Caches.erase(ranges::remove_if(Caches, [&](const FolderCache &cache) {
		return !cache.session || cache.session.get() == session;
	}), Caches.end());
}

const FolderCache &Cache(not_null<Main::Session*> session) {
	const auto i = ranges::find_if(Caches, [&](const FolderCache &cache) {
		return cache.session.get() == session;
	});
	if (i != Caches.end()) {
		return *i;
	}
	ResetCache(session);
	auto &cache = Caches.emplace_back();
	cache.session = base::make_weak(session);
	for (const auto &id : ReadRecent(session)) {
		cache.peers.emplace(id);
	}
	for (const auto &id : ReadRecentFolders(session)) {
		cache.folders.emplace(id);
	}
	return cache;
}

void RefreshFolders(
		not_null<Main::Session*> session,
		const std::vector<PeerId> &ids) {
	const auto owner = &session->data();
	for (const auto &id : ids) {
		if (const auto history = owner->historyLoaded(id)) {
			owner->chatsFilters().refreshHistory(history);
		}
	}
}

void WriteRecent(
		not_null<Main::Session*> session,
		const std::vector<PeerId> &ids) {
	const auto previous = ReadRecent(session);
	auto parts = QStringList();
	for (const auto &id : ids) {
		parts.push_back(QString::number(id.value));
	}
	Expects(ForAccount(session).Set(kRecentChatsList, parts.join(u',')));
	ResetCache(session);
	if (ReadRecentFolders(session).empty()) {
		return;
	}
	auto changed = std::vector<PeerId>();
	for (const auto &id : previous) {
		if (!ranges::contains(ids, id)) {
			changed.push_back(id);
		}
	}
	for (const auto &id : ids) {
		if (!ranges::contains(previous, id)) {
			changed.push_back(id);
		}
	}
	RefreshFolders(session, changed);
}

void Remember(not_null<PeerData*> peer) {
	const auto session = &peer->session();
	auto ids = ReadRecent(session);
	ids.erase(ranges::remove(ids, peer->id), ids.end());
	ids.insert(ids.begin(), peer->id);
	const auto limit = ForDevice().Get(kRecentChatsLimit);
	if (int(ids.size()) > limit) {
		ids.resize(limit);
	}
	WriteRecent(session, ids);
}

class RecentFilterRow final : public PeerListRow {
public:
	RecentFilterRow() : PeerListRow(kRecentFilterRowId) {
	}

	QString generateName() override {
		return tr::lng_nagram_recent_chats(tr::now);
	}
	QString generateShortName() override {
		return generateName();
	}
	PaintRoundImageCallback generatePaintUserpicCallback(
			bool forceRound) override {
		return [](QPainter &p, int x, int y, int outerWidth, int size) {
			const auto rect = style::rtlrect(x, y, size, size, outerWidth);
			auto hq = PainterHighQualityEnabler(p);
			auto bg = QLinearGradient(x, y, x, y + size);
			bg.setStops({
				{ 0., st::historyPeer3UserpicBg->c },
				{ 1., st::historyPeer3UserpicBg2->c },
			});
			p.setBrush(bg);
			p.setPen(Qt::NoPen);
			p.drawEllipse(rect);
			st::nagramFilterTypeRecent.paintInCenter(p, rect);
		};
	}

};

class RecentChatsController final : public PeerListController {
public:
	explicit RecentChatsController(
		not_null<Window::SessionController*> window)
	: _window(window) {
	}

	Main::Session &session() const override {
		return _window->session();
	}

	void prepare() override {
		auto &owner = session().data();
		const auto current = _window->activeChatCurrent().peer();
		for (const auto &id : ReadRecent(&session())) {
			const auto peer = owner.peerLoaded(id);
			if (peer && peer != current) {
				delegate()->peerListAppendRow(
					std::make_unique<PeerListRow>(peer));
			}
		}
		if (!delegate()->peerListFullRowsCount()) {
			setDescriptionText(tr::lng_nagram_recent_chats_empty(tr::now));
		}
		delegate()->peerListRefreshRows();
	}

	void rowClicked(not_null<PeerListRow*> row) override {
		const auto peer = row->peer();
		const auto window = _window;
		window->hideLayer();
		window->showPeerHistory(peer, Window::SectionShow::Way::ClearStack);
	}

private:
	const not_null<Window::SessionController*> _window;

};

} // namespace

void WatchRecentChats(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	controller->activeChatChanges(
	) | rpl::filter([](const Dialogs::Key &key) {
		return key.peer() && ForDevice().Get(kRecentChats);
	}) | rpl::on_next([](const Dialogs::Key &key) {
		Remember(key.peer());
	}, controller->lifetime());
	ForDevice().Value(kRecentChats) | rpl::filter([](bool enabled) {
		return !enabled;
	}) | rpl::on_next([=] {
		WriteRecent(session, {});
	}, controller->lifetime());
	ForDevice().Value(kRecentChatsLimit) | rpl::on_next([=](int limit) {
		auto ids = ReadRecent(session);
		if (int(ids.size()) > limit) {
			ids.resize(limit);
			WriteRecent(session, ids);
		}
	}, controller->lifetime());
}

void ForEachRecentShareTarget(
		not_null<Main::Session*> session,
		Fn<void(not_null<History*>)> callback) {
	if (!ForDevice().Get(kRecentChats) || !ForDevice().Get(kRecentInShare)) {
		return;
	}
	auto &owner = session->data();
	for (const auto &id : ReadRecent(session)) {
		if (const auto peer = owner.peerLoaded(id); peer && !peer->isSelf()) {
			callback(owner.history(peer));
		}
	}
}

void ForEachRecentChat(
		not_null<Main::Session*> session,
		Fn<void(not_null<History*>)> callback) {
	if (!ForDevice().Get(kRecentChats)) {
		return;
	}
	auto &owner = session->data();
	for (const auto &id : ReadRecent(session)) {
		if (const auto peer = owner.peerLoaded(id)) {
			callback(owner.history(peer));
		}
	}
}

std::unique_ptr<PeerListRow> MakeRecentFilterRow() {
	return ForDevice().Get(kRecentChats)
		? std::make_unique<RecentFilterRow>()
		: nullptr;
}

bool RecentInFolder(not_null<History*> history, FilterId folderId) {
	if (!folderId) {
		return false;
	}
	const auto &cache = Cache(&history->session());
	return cache.folders.contains(folderId)
		&& cache.peers.contains(history->peer->id);
}

bool RecentFolderEnabled(
		not_null<Main::Session*> session,
		FilterId folderId) {
	return ranges::contains(ReadRecentFolders(session), folderId);
}

void SetRecentFolderEnabled(
		not_null<Main::Session*> session,
		FilterId folderId,
		bool enabled) {
	auto ids = ReadRecentFolders(session);
	if (!folderId || ranges::contains(ids, folderId) == enabled) {
		return;
	} else if (enabled) {
		ids.push_back(folderId);
		ranges::sort(ids);
	} else {
		ids.erase(ranges::remove(ids, folderId), ids.end());
	}
	auto parts = QStringList();
	for (const auto &id : ids) {
		parts.push_back(QString::number(id));
	}
	Expects(ForAccount(session).Set(kRecentFolderIds, parts.join(u',')));
	ResetCache(session);
	RefreshFolders(session, ReadRecent(session));
}

void ShowRecentChats(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	controller->show(Box<PeerListBox>(
		std::make_unique<RecentChatsController>(controller),
		[=](not_null<PeerListBox*> box) {
			box->setTitle(tr::lng_nagram_recent_chats());
			box->addButton(tr::lng_close(), [=] { box->closeBox(); });
			box->addLeftButton(tr::lng_nagram_recent_chats_clear(), [=] {
				WriteRecent(session, {});
				box->closeBox();
			});
		}));
}

} // namespace Nagram::Chats
