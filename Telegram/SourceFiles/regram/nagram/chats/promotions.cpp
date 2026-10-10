#include "regram/nagram/chats/promotions.h"

#include "regram/nagram/chats/options.h"
#include "dialogs/suggestions/suggestion.h"

namespace Nagram::Chats {

bool HideSponsoredMessages() {
	return ForDevice().Get(kHideSponsoredMessages);
}

bool HideProxySponsor() {
	return ForDevice().Get(kHideProxySponsor);
}

bool HidePhoneSuggestion() {
	return ForDevice().Get(kHidePhoneSuggestion);
}

bool HideSuggestion(Dialogs::TopBarSuggestions::Priority priority) {
	using Priority = Dialogs::TopBarSuggestions::Priority;
	switch (priority) {
	case Priority::BirthdaySetup:
	case Priority::BirthdayContacts:
		return ForDevice().Get(kHideBirthdaySuggestions);
	case Priority::PremiumOffer:
	case Priority::PremiumGrace:
	case Priority::LowCreditsSubs:
	case Priority::CustomPromo:
	case Priority::GiftAuctions:
		return ForDevice().Get(kHidePremiumPromotions);
	default:
		return false;
	}
}

} // namespace Nagram::Chats
