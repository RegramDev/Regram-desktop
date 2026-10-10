#pragma once

#include "info/profile/info_profile_badge.h"

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Nagram::Messages {

[[nodiscard]] bool ShowPremiumBadges();
[[nodiscard]] rpl::producer<Info::Profile::Badge::Content> VisibleBadgeContent(
	rpl::producer<Info::Profile::Badge::Content> content);
void AttachPremiumRefresh(not_null<Ui::RpWidget*> widget);

} // namespace Nagram::Messages
