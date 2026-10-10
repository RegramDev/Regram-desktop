#include "regram/nagram/chats/community.h"

#include "regram/nagram/chats/options.h"

namespace Nagram::Chats {

bool CommunityGroupingDisabled() {
	// Read once: chat list membership is built from it at startup.
	static const auto result = ForDevice().Get(kDisableCommunityGrouping);
	return result;
}

} // namespace Nagram::Chats
