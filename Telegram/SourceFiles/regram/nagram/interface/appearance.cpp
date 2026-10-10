#include "regram/nagram/interface/appearance.h"

#include "regram/nagram/interface/options.h"
#include "data/data_peer.h"

namespace Nagram::Interface {

rpl::producer<bool> IgnoreChatThemeValue(not_null<PeerData*> peer) {
	const auto user = peer->isUser();
	const auto broadcast = peer->isBroadcast();
	return rpl::combine(
		ForDevice().Value(kIgnoreChatTheme),
		ForDevice().Value(kIgnorePrivateChatTheme),
		ForDevice().Value(kIgnoreChannelChatTheme)
	) | rpl::map([=](bool all, bool privates, bool channels) {
		return all || (user && privates) || (broadcast && channels);
	});
}

bool AccountNameInTitle() {
	return ForDevice().Get(kAccountNameInTitle);
}

bool AlwaysSeasonal() {
	return ForDevice().Get(kAlwaysSeasonal);
}

} // namespace Nagram::Interface
