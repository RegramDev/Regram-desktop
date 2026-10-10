#include "regram/nagram/messages/badges.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/ayu/ayu_settings.h"
#include "ui/rp_widget.h"

namespace Nagram::Messages {

bool ShowPremiumBadges() {
	return !ForDevice().Get(kHidePremiumBadges);
}

rpl::producer<Info::Profile::Badge::Content> VisibleBadgeContent(
		rpl::producer<Info::Profile::Badge::Content> content) {
	return rpl::combine(
		std::move(content),
		ForDevice().Value(kHidePremiumBadges)
	) | rpl::map([](Info::Profile::Badge::Content content, bool hidden) {
		if (hidden) {
			if (content.badge == Info::Profile::BadgeType::Premium) {
				content.badge = Info::Profile::BadgeType::None;
			}
			if (content.badge != Info::Profile::BadgeType::BotVerified) {
				content.emojiStatusId = {};
			}
		}
		return content;
	});
}

void AttachPremiumRefresh(not_null<Ui::RpWidget*> widget) {
	ForDevice().changes(
	) | rpl::filter([](std::string_view key) {
		return key == kHidePremiumBadges.key;
	}) | rpl::on_next([widget](std::string_view) {
		widget->update();
	}, widget->lifetime());
	AyuSettings::getInstance().localPremiumChanges()
	| rpl::on_next([widget] { widget->update(); }, widget->lifetime());
}

} // namespace Nagram::Messages
