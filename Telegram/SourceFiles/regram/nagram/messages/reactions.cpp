#include "regram/nagram/messages/reactions.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/messages/content.h"
#include "data/data_peer.h"
#include "data/data_message_reactions.h"
#include "history/history_item.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "history/view/reactions/history_view_reactions.h"
#include "history/view/reactions/history_view_reactions_button.h"

namespace Nagram::Messages {

bool ReactionsHidden(not_null<const PeerData*> peer) {
	auto &options = ForDevice();
	return options.Get(kHideReactions)
		|| options.Get(peer->isUser()
			? kHidePrivateReactions
			: peer->isBroadcast()
			? kHideChannelReactions
			: kHideGroupReactions);
}

HistoryView::Reactions::InlineListData FilterInlineReactions(
		not_null<HistoryView::Element*> view,
		HistoryView::Reactions::InlineListData data) {
	if (ReactionsHidden(view->data()->history()->peer)) {
		data.reactions.clear();
	}
	return data;
}

HistoryView::Reactions::ButtonParameters FilterReactionButton(
		not_null<const PeerData*> peer,
		HistoryView::Reactions::ButtonParameters parameters) {
	return ReactionsHidden(peer)
		? HistoryView::Reactions::ButtonParameters()
		: parameters;
}

Data::PossibleItemReactionsRef MenuReactions(
		not_null<HistoryItem*> item) {
	return (ForDevice().Get(kHideReactionMenu)
		|| (item->reactionsAreTags() && HideSavedTags()))
		? Data::PossibleItemReactionsRef()
		: Data::LookupPossibleReactions(item, true);
}

bool AllowReactionSelector(bool hasSelection) {
	return !hasSelection
		|| !ForDevice().Get(kHideReactionMenuWhenSelecting);
}

bool WhoReactedAtTop() {
	return ForDevice().Get(kMoveWhoReactedToTop);
}

} // namespace Nagram::Messages
