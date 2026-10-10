#include "regram/nagram/compose/placeholder.h"

#include "regram/nagram/compose/options.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/send_as_peers.h"

namespace Nagram::Compose {

rpl::producer<QString> InputPlaceholder(not_null<PeerData*> peer) {
	auto refresh = rpl::merge(
		peer->session().sendAsPeers().updated() | rpl::map_to(0),
		peer->session().changes().peerUpdates(
			Data::PeerUpdate::Flag::Name
		) | rpl::filter([peer](const Data::PeerUpdate &update) {
			return update.peer == peer;
		}) | rpl::map_to(0),
		tr::lng_message_ph() | rpl::map_to(0));
	return rpl::combine(
		ForDevice().Value(kInputPlaceholderMode),
		rpl::single(0) | rpl::then(std::move(refresh))
	) | rpl::map([peer](int mode, int) {
		if (mode == 1) {
			return tr::lng_nagram_input_chat_hint(
				tr::now, lt_name, peer->name());
		} else if (mode == 2) {
			return tr::lng_nagram_input_sender_hint(
				tr::now,
				lt_name,
				peer->session().sendAsPeers().resolveChosen(peer)->name());
		}
		return tr::lng_message_ph(tr::now);
	}) | rpl::distinct_until_changed();
}

} // namespace Nagram::Compose
