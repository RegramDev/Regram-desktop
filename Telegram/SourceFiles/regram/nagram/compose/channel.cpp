#include "regram/nagram/compose/channel.h"

#include "regram/nagram/compose/buttons.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"

namespace Nagram::Compose {

ChannelData *DiscussionTarget(PeerData *peer) {
	const auto channel = peer ? peer->asBroadcast() : nullptr;
	return (channel && ForDevice().Get(kChannelDiscussButton))
		? channel->discussionLink()
		: nullptr;
}

bool ChannelBottomVisible(PeerData *peer) {
	return !Hidden(kHideChannelMuteButton) || DiscussionTarget(peer);
}

QString ChannelBottomText(PeerData *peer, QString muteText) {
	return DiscussionTarget(peer)
		? tr::lng_profile_view_discussion(tr::now)
		: muteText;
}

bool OpenDiscussion(
		Window::SessionController *controller,
		PeerData *peer) {
	const auto chat = DiscussionTarget(peer);
	if (!controller || !chat) {
		return false;
	}
	if (peer->asChannel()->invitePeekExpires()) {
		controller->showToast(tr::lng_channel_invite_private(tr::now));
	} else {
		controller->showPeerHistory(
			chat,
			Window::SectionShow::Way::Forward);
	}
	return true;
}

} // namespace Nagram::Compose
