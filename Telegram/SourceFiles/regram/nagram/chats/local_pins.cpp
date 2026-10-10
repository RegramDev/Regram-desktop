#include "regram/nagram/chats/local_pins.h"

#include "regram/nagram/chats/local_pins_model.h"
#include "base/weak_ptr.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_entry.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <map>
#include <memory>

namespace Nagram::Chats {
namespace {

struct State {
	base::weak_ptr<Main::Session> guard;
	std::vector<quint64> peers;
	base::flat_map<quint64, int> indices;
	rpl::event_stream<> changes;
};

auto Enabled = false;
auto EnabledKnown = false;

[[nodiscard]] bool IsEnabled() {
	if (!EnabledKnown) {
		EnabledKnown = true;
		Enabled = ForDevice().Get(kUnlimitedPinnedChats);
	}
	return Enabled;
}

void Reindex(State &state) {
	state.indices.clear();
	for (auto i = 0; i != int(state.peers.size()); ++i) {
		state.indices.emplace(state.peers[i], i);
	}
}

void Watch(not_null<Main::Session*> session);

[[nodiscard]] State &ForSession(not_null<Main::Session*> session) {
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	if (const auto i = states.find(session)
		; i != states.end() && i->second->guard) {
		return *i->second;
	}
	std::erase_if(states, [](const auto &entry) {
		return !entry.second->guard;
	});
	auto state = std::make_unique<State>();
	state->guard = base::make_weak(session);
	state->peers = ParseLocalPins(
		ForAccount(session).Get(kLocalPinnedChats),
		session->userId().bare);
	Reindex(*state);
	// WHY: sort keys are computed while the session is still being built,
	// so the subscriptions wait until it is complete.
	crl::on_main(session, [=] { Watch(session); });
	return *states.emplace(session, std::move(state)).first->second;
}

void RefreshRows(
		not_null<Main::Session*> session,
		const std::vector<quint64> &peers) {
	for (const auto &id : peers) {
		if (const auto history = session->data().historyLoaded(PeerId(id))) {
			history->updateChatListSortPosition();
			history->updateChatListEntry();
		}
	}
}

void Save(not_null<Main::Session*> session, std::vector<quint64> peers) {
	auto &state = ForSession(session);
	auto touched = state.peers;
	touched.insert(touched.end(), peers.begin(), peers.end());
	state.peers = std::move(peers);
	Reindex(state);
	Expects(ForAccount(session).Set(
		kLocalPinnedChats,
		SerializeLocalPins(state.peers, session->userId().bare)));
	RefreshRows(session, touched);
	state.changes.fire({});
}

void MergeWithServer(not_null<Main::Session*> session) {
	auto peers = ForSession(session).peers;
	auto pinned = std::vector<quint64>();
	for (const auto &id : peers) {
		const auto history = session->data().historyLoaded(PeerId(id));
		if (history && history->isPinnedDialog(FilterId())) {
			pinned.push_back(id);
		}
	}
	if (MergeLocalPins(peers, pinned)) {
		Save(session, std::move(peers));
	}
}

void Watch(not_null<Main::Session*> session) {
	ForDevice().Value(
		kUnlimitedPinnedChats
	) | rpl::on_next([=](bool enabled) {
		EnabledKnown = true;
		Enabled = enabled;
		if (enabled) {
			MergeWithServer(session);
		}
		RefreshRows(session, ForSession(session).peers);
	}, session->lifetime());

	session->data().pinnedDialogsOrderUpdated(
	) | rpl::filter([] {
		return IsEnabled();
	}) | rpl::on_next([=] {
		MergeWithServer(session);
	}, session->lifetime());
}

[[nodiscard]] bool LocalPinned(not_null<const Dialogs::Entry*> entry) {
	if (!IsEnabled()) {
		return false;
	}
	const auto history = entry->asHistory();
	return history
		&& !entry->isPinnedDialog(FilterId())
		&& ForSession(&history->session()).indices.contains(
			history->peer->id.value);
}

[[nodiscard]] bool AcceptsLocalPin(not_null<History*> history) {
	const auto channel = history->peer->asChannel();
	return !history->fixedOnTopIndex()
		&& (!channel || !channel->isCommunity());
}

} // namespace

uint64 LocalPinSortKey(
		const Dialogs::Entry &entry,
		FilterId filterId,
		uint64 original) {
	if (filterId || !original || !IsEnabled()) {
		return original;
	}
	const auto history = entry.asHistory();
	if (!history) {
		return original;
	}
	const auto &indices = ForSession(&history->session()).indices;
	const auto i = indices.find(history->peer->id.value);
	return (i != end(indices)) ? LocalPinSortKeyFor(i->second) : original;
}

bool ShowsPinnedIcon(
		not_null<const Dialogs::Entry*> entry,
		FilterId filterId) {
	return entry->isPinnedDialog(filterId)
		|| (!filterId && LocalPinned(entry));
}

bool TryLocalPin(
		not_null<Window::SessionController*> controller,
		not_null<History*> history) {
	if (!IsEnabled() || !AcceptsLocalPin(history)) {
		return false;
	}
	const auto session = &history->session();
	auto peers = ForSession(session).peers;
	if (!AddLocalPin(peers, history->peer->id.value)) {
		controller->showToast(tr::lng_nagram_local_pin_limit(
			tr::now,
			lt_limit,
			QString::number(kLocalPinsLimit)));
		return true;
	}
	Save(session, std::move(peers));
	controller->showToast(tr::lng_nagram_local_pin_done(tr::now));
	return true;
}

bool LocalUnpin(
		not_null<Dialogs::Entry*> entry,
		const Fn<void()> &onToggled) {
	if (!LocalPinned(entry)) {
		return false;
	}
	const auto history = entry->asHistory();
	const auto session = &history->session();
	auto peers = ForSession(session).peers;
	if (RemoveLocalPin(peers, history->peer->id.value)) {
		Save(session, std::move(peers));
	}
	if (onToggled) {
		onToggled();
	}
	return true;
}

bool AddLocalUnpinAction(
		const Ui::Menu::MenuCallback &addAction,
		not_null<Dialogs::Entry*> entry,
		FilterId filterId) {
	if (filterId || !LocalPinned(entry)) {
		return false;
	}
	const auto weak = base::make_weak(entry);
	addAction(tr::lng_nagram_local_unpin(tr::now), [=] {
		if (const auto strong = weak.get()) {
			[[maybe_unused]] const auto done = LocalUnpin(strong, nullptr);
		}
	}, &st::menuIconUnpin);
	return true;
}

rpl::producer<int> LocalPinsCountValue(not_null<Main::Session*> session) {
	return rpl::single(rpl::empty) | rpl::then(
		ForSession(session).changes.events()
	) | rpl::map([=] {
		return int(ForSession(session).peers.size());
	}) | rpl::distinct_until_changed();
}

void LocalPinsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_local_pinned_chats());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_local_pinned_chats_about(),
		st::boxLabel));
	const auto rows = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		style::margins());
	const auto refresh = [=] {
		rows->clear();
		for (const auto &id : ForSession(session).peers) {
			const auto peer = session->data().peerLoaded(PeerId(id));
			const auto history = session->data().historyLoaded(PeerId(id));
			const auto listed = history && history->inChatList();
			const auto row = Settings::AddButtonWithLabel(
				rows,
				rpl::single(peer ? peer->name() : QString::number(id)),
				listed
					? rpl::single(QString())
					: tr::lng_nagram_local_pinned_missing(),
				st::settingsButtonNoIcon);
			row->setClickedCallback([=] {
				crl::on_main(box, [=] {
					auto peers = ForSession(session).peers;
					if (RemoveLocalPin(peers, id)) {
						Save(session, std::move(peers));
					}
				});
			});
		}
		rows->resizeToWidth(box->width());
	};
	rpl::single(rpl::empty) | rpl::then(
		ForSession(session).changes.events()
	) | rpl::on_next(refresh, rows->lifetime());
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(tr::lng_nagram_local_pinned_chats_clear_all(), [=] {
		box->uiShow()->showBox(Ui::MakeConfirmBox({
			.text = tr::lng_nagram_local_pinned_chats_clear(),
			.confirmed = [=](Fn<void()> &&close) {
				Save(session, {});
				close();
			},
		}));
	});
}

} // namespace Nagram::Chats
