#pragma once

#include <rpl/producer.h>

namespace Main {
class Session;
} // namespace Main

namespace Regram::Premium {

[[nodiscard]] bool ForLocalFeatures(const Main::Session &session);
[[nodiscard]] rpl::producer<bool> ForLocalFeaturesValue(
	not_null<Main::Session*> session);

} // namespace Regram::Premium
