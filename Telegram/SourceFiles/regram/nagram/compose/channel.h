#pragma once

#include <QtCore/QString>

class PeerData;
class ChannelData;
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Compose {

[[nodiscard]] ChannelData *DiscussionTarget(PeerData *peer);
[[nodiscard]] bool ChannelBottomVisible(PeerData *peer);
[[nodiscard]] QString ChannelBottomText(PeerData *peer, QString muteText);
[[nodiscard]] bool OpenDiscussion(
	Window::SessionController *controller,
	PeerData *peer);

} // namespace Nagram::Compose
