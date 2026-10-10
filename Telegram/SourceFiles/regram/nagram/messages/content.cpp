#include "regram/nagram/messages/content.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "data/data_message_reaction_id.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "history/history_item.h"
#include "history/view/media/history_view_similar_channels.h"
#include "ui/rp_widget.h"

#include <algorithm>

namespace Nagram::Messages {

bool RevealTextSpoilers() {
	return ForDevice().Get(kRevealSpoilers);
}

bool RevealMediaSpoiler(not_null<HistoryItem*> item) {
	return ForDevice().Get(kRevealSpoilers)
		&& !item->isMediaSensitive()
		&& !item->isTtlCoveredMedia()
		&& !(item->media() && item->media()->invoice());
}

bool HideQuickShare() {
	return ForDevice().Get(kHideQuickShare);
}

bool HideRecommendedChannels() {
	return ForDevice().Get(kHideRecommendedChannels);
}

std::unique_ptr<HistoryView::Media> RecommendedChannelsMedia(
		not_null<HistoryView::Element*> view) {
	return HideRecommendedChannels()
		? nullptr
		: std::make_unique<HistoryView::SimilarChannels>(view);
}

bool CollapseRecommendedChannels() {
	return ForDevice().Get(kCollapseRecommendedChannels);
}

bool SimplifiedReplies() {
	return ForDevice().Get(kSimplifiedReplies);
}

bool VideoUserpicAllowed(not_null<PeerData*> peer) {
	return peer->userpicHasVideo()
		&& !ForDevice().Get(kDisableVideoUserpics);
}

bool HideSavedTags() {
	return ForDevice().Get(kHideSavedTags);
}

rpl::producer<bool> SavedTagsValue() {
	return ForDevice().Value(kHideSavedTags);
}

bool ShowSavedTag(
		Data::ReactionId id,
		const std::vector<Data::ReactionId> &selected,
		const std::vector<Data::ReactionId> &added) {
	return !HideSavedTags()
		|| std::find(selected.begin(), selected.end(), id) != selected.end()
		|| std::find(added.begin(), added.end(), id) != added.end();
}

bool HidePrivateActivity(const PeerData *peer) {
	return peer && peer->isUser()
		&& ForDevice().Get(kHidePrivateChatActivities);
}

void AttachActivityRefresh(not_null<Ui::RpWidget*> widget) {
	ForDevice().changes(
	) | rpl::filter([](std::string_view key) {
		return key == kHidePrivateChatActivities.key;
	}) | rpl::on_next([widget](std::string_view) {
		widget->update();
	}, widget->lifetime());
}

} // namespace Nagram::Messages
