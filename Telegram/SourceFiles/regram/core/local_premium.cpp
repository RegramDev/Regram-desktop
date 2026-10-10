#include "regram/core/local_premium.h"

#include "regram/ayu/ayu_settings.h"
#include "data/data_peer_values.h"
#include "data/data_user.h"
#include "main/main_session.h"

namespace Regram::Premium {

bool ForLocalFeatures(const Main::Session &session) {
	return session.premium() || AyuSettings::getInstance().localPremium();
}

rpl::producer<bool> ForLocalFeaturesValue(not_null<Main::Session*> session) {
	return rpl::combine(
		Data::PeerPremiumValue(session->user()),
		AyuSettings::getInstance().localPremiumValue(),
		rpl::mappers::_1 || rpl::mappers::_2);
}

} // namespace Regram::Premium
