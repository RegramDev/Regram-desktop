#pragma once

#include "data/data_types.h"

#include <rpl/producer.h>
#include <optional>

class PeerData;
struct EmojiStatusId;

namespace Main {
class Session;
} // namespace Main

namespace Regram::Premium {

void RegisterAccount(not_null<Main::Session*> session);
[[nodiscard]] bool ForLocalFeatures(const Main::Session &session);
[[nodiscard]] rpl::producer<bool> ForLocalFeaturesValue(
	not_null<Main::Session*> session);
[[nodiscard]] bool ForPeerBadge(not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<bool> ForPeerBadgeValue(not_null<PeerData*> peer);
[[nodiscard]] std::optional<EmojiStatusId> LocalEmojiStatus(
	not_null<const PeerData*> peer);
[[nodiscard]] bool StoreLocalEmojiStatus(
	not_null<PeerData*> peer,
	EmojiStatusId status,
	TimeId until);

} // namespace Regram::Premium
