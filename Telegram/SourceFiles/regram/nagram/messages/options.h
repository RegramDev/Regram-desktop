#pragma once

#include "regram/nagram/core/options.h"

namespace Nagram::Messages {

inline constexpr auto kRefreshMessageView = static_cast<unsigned>(
	Flag::RefreshMessageView);

inline constexpr auto kSecondsInMessages = Option<bool>{
	"nagram.secondsInMessages", Scope::Device, false,
	Category::Messages, "lng_nagram_seconds_in_messages", kRefreshMessageView };
inline constexpr auto kShowForwardedMessageDate = Option<bool>{
	"nagram.showForwardedMessageDate", Scope::Device, false,
	Category::Messages, "lng_nagram_show_forwarded_message_date", kRefreshMessageView };
inline constexpr auto kShowServiceTime = Option<bool>{
	"nagram.showServiceTime", Scope::Device, false,
	Category::Messages, "lng_nagram_show_service_time", kRefreshMessageView };
inline constexpr auto kShowMessageId = Option<bool>{
	"nagram.showMessageId", Scope::Device, false,
	Category::Messages, "lng_nagram_show_message_id", kRefreshMessageView };
inline constexpr auto kSenderOnlineStatus = Option<int>{
	"nagram.senderOnlineStatus", Scope::Device, 0,
	Category::Messages, "lng_nagram_sender_online", kRefreshMessageView,
	[](const int &value) { return value >= 0 && value <= 2; } };
inline constexpr auto kExactMessageCounters = Option<bool>{
	"nagram.exactMessageCounters", Scope::Device, false,
	Category::Messages, "lng_nagram_exact_message_counters", kRefreshMessageView };
inline constexpr auto kHideMessageViews = Option<bool>{
	"nagram.hideMessageViews", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_message_views", kRefreshMessageView };
inline constexpr auto kShowForwardCount = Option<bool>{
	"nagram.showForwardCount", Scope::Device, false,
	Category::Messages, "lng_nagram_show_forward_count", kRefreshMessageView };
inline constexpr auto kHideChannelSignature = Option<bool>{
	"nagram.hideChannelSignature", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_channel_signature", kRefreshMessageView };
inline constexpr auto kHideEditedBadge = Option<bool>{
	"nagram.hideEditedBadge", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_edited_badge", kRefreshMessageView };
inline const auto kEditedMark = Option<QString>{
	"nagram.editedMark", Scope::Device, QString(),
	Category::Messages, "lng_nagram_edited_mark", kRefreshMessageView };
inline constexpr auto kHideReactions = Option<bool>{
	"nagram.hideReactions", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_reactions", kRefreshMessageView };
inline constexpr auto kHidePrivateReactions = Option<bool>{
	"nagram.hidePrivateReactions", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_private_reactions", kRefreshMessageView };
inline constexpr auto kHideGroupReactions = Option<bool>{
	"nagram.hideGroupReactions", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_group_reactions", kRefreshMessageView };
inline constexpr auto kHideChannelReactions = Option<bool>{
	"nagram.hideChannelReactions", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_channel_reactions", kRefreshMessageView };
inline constexpr auto kHideReactionMenu = Option<bool>{
	"nagram.hideReactionMenu", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_reaction_menu" };
inline constexpr auto kHideReactionMenuWhenSelecting = Option<bool>{
	"nagram.hideReactionMenuWhenSelecting", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_reaction_menu_when_selecting" };
inline constexpr auto kMoveWhoReactedToTop = Option<bool>{
	"nagram.moveWhoReactedToTop", Scope::Device, false,
	Category::Messages, "lng_nagram_move_who_reacted_to_top" };
inline constexpr auto kDisablePremiumStickerEffects = Option<bool>{
	"nagram.disablePremiumStickerEffects", Scope::Device, false,
	Category::Messages, "lng_nagram_disable_premium_sticker_effects" };
inline constexpr auto kDisableEmojiInteractions = Option<bool>{
	"nagram.disableEmojiInteractions", Scope::Device, false,
	Category::Messages, "lng_nagram_disable_emoji_interactions" };
inline constexpr auto kDisableMessageEffects = Option<bool>{
	"nagram.disableMessageEffects", Scope::Device, false,
	Category::Messages, "lng_nagram_disable_message_effects" };
inline constexpr auto kRevealSpoilers = Option<bool>{
	"nagram.revealSpoilers", Scope::Device, false,
	Category::Messages, "lng_nagram_reveal_spoilers", kRefreshMessageView };
inline constexpr auto kHideQuickShare = Option<bool>{
	"nagram.hideQuickShare", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_quick_share", kRefreshMessageView };
inline constexpr auto kHideRecommendedChannels = Option<bool>{
	"nagram.hideRecommendedChannels", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_recommended_channels", kRefreshMessageView };
inline constexpr auto kHidePremiumBadges = Option<bool>{
	"nagram.hidePremiumBadges", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_premium_badges", kRefreshMessageView };
inline constexpr auto kEditedIcon = Option<bool>{
	"nagram.editedIcon", Scope::Device, false,
	Category::Messages, "lng_nagram_edited_icon", kRefreshMessageView };
inline constexpr auto kSimplifiedReplies = Option<bool>{
	"nagram.simplifiedReplies", Scope::Device, false,
	Category::Messages, "lng_nagram_simplified_replies", kRefreshMessageView };
inline constexpr auto kDisableVideoUserpics = Option<bool>{
	"nagram.disableVideoUserpics", Scope::Device, false,
	Category::Messages, "lng_nagram_disable_video_userpics", kRefreshMessageView
		| static_cast<unsigned>(Flag::RefreshDialogList) };
inline constexpr auto kCollapseRecommendedChannels = Option<bool>{
	"nagram.collapseRecommendedChannels", Scope::Device, false,
	Category::Messages, "lng_nagram_collapse_recommended_channels" };
inline constexpr auto kHideSavedTags = Option<bool>{
	"nagram.hideSavedTags", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_saved_tags" };
inline constexpr auto kHidePrivateChatActivities = Option<bool>{
	"nagram.hidePrivateChatActivities", Scope::Device, false,
	Category::Messages, "lng_nagram_hide_private_chat_activities" };
inline constexpr auto kReadingSpacing = Option<bool>{
	"nagram.readingSpacing", Scope::Device, false,
	Category::Messages, "lng_nagram_reading_spacing", kRefreshMessageView };
inline constexpr auto kReadingChinese = Option<int>{
	"nagram.readingChinese", Scope::Device, 0,
	Category::Messages, "lng_nagram_reading_chinese", kRefreshMessageView,
	[](const int &value) { return value >= 0 && value <= 2; } };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kSecondsInMessages));
	Expects(registry.Add(kShowForwardedMessageDate));
	Expects(registry.Add(kShowServiceTime));
	Expects(registry.Add(kShowMessageId));
	Expects(registry.Add(kSenderOnlineStatus));
	Expects(registry.Add(kExactMessageCounters));
	Expects(registry.Add(kHideMessageViews));
	Expects(registry.Add(kShowForwardCount));
	Expects(registry.Add(kHideChannelSignature));
	Expects(registry.Add(kHideEditedBadge));
	Expects(registry.Add(kEditedMark));
	Expects(registry.Add(kHideReactions));
	Expects(registry.Add(kHidePrivateReactions));
	Expects(registry.Add(kHideGroupReactions));
	Expects(registry.Add(kHideChannelReactions));
	Expects(registry.Add(kHideReactionMenu));
	Expects(registry.Add(kHideReactionMenuWhenSelecting));
	Expects(registry.Add(kMoveWhoReactedToTop));
	Expects(registry.Add(kDisablePremiumStickerEffects));
	Expects(registry.Add(kDisableEmojiInteractions));
	Expects(registry.Add(kDisableMessageEffects));
	Expects(registry.Add(kRevealSpoilers));
	Expects(registry.Add(kHideQuickShare));
	Expects(registry.Add(kHideRecommendedChannels));
	Expects(registry.Add(kHidePremiumBadges));
	Expects(registry.Add(kHideSavedTags));
	Expects(registry.Add(kEditedIcon));
	Expects(registry.Add(kSimplifiedReplies));
	Expects(registry.Add(kDisableVideoUserpics));
	Expects(registry.Add(kCollapseRecommendedChannels));
	Expects(registry.Add(kHidePrivateChatActivities));
	Expects(registry.Add(kReadingSpacing));
	Expects(registry.Add(kReadingChinese));
}

} // namespace Nagram::Messages
