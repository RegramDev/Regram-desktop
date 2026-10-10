#pragma once

#include "regram/nagram/core/options.h"

#include <array>

namespace Nagram::Interface {

[[nodiscard]] bool ValidMainMenuBytes(const QByteArray &value);

inline constexpr auto kAppIconIds = std::array{
	u"block",
	u"block_black",
	u"block_blue",
	u"block_niello",
	u"block_purple",
	u"classic",
	u"colorful",
	u"cyan",
	u"black",
};
[[nodiscard]] inline bool ValidAppIcon(const QString &value) {
	if (value.isEmpty()) {
		return true;
	}
	for (const auto id : kAppIconIds) {
		if (value == QStringView(id)) {
			return true;
		}
	}
	return false;
}

inline constexpr auto kRestart = static_cast<unsigned>(Flag::RequiresRestart);
inline constexpr auto kBubbleRoundness = Option<int>{
	"nagram.bubbleRoundness", Scope::Device, 0,
	Category::Interface, "lng_nagram_bubble_roundness", kRestart,
	[](const int &value) { return !value || (value >= 10 && value <= 100); } };
inline constexpr auto kAvatarRoundness = Option<int>{
	"nagram.avatarRoundness", Scope::Device, 0,
	Category::Interface, "lng_nagram_avatar_roundness", kRestart,
	[](const int &value) { return !value || (value >= 10 && value <= 100); } };
inline constexpr auto kUniformAvatarShapes = Option<bool>{
	"nagram.uniformAvatarShapes", Scope::Device, false,
	Category::Interface, "lng_nagram_uniform_avatar_shapes", kRestart };
inline constexpr auto kTextMessageWidth = Option<int>{
	"nagram.textMessageWidth", Scope::Device, 0,
	Category::Interface, "lng_nagram_text_message_width",
	static_cast<unsigned>(Flag::RefreshMessageView),
	[](const int &value) { return !value || (value >= 50 && value <= 400); } };
inline constexpr auto kWideChannelPosts = Option<bool>{
	"nagram.wideChannelPosts", Scope::Device, false,
	Category::Interface, "lng_nagram_wide_channel_posts",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kHideBubbleTail = Option<bool>{
	"nagram.hideBubbleTail", Scope::Device, false,
	Category::Interface, "lng_nagram_hide_bubble_tail",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kThemeReplyColors = Option<bool>{
	"nagram.themeReplyColors", Scope::Device, false,
	Category::Interface, "lng_nagram_theme_reply_colors",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kHideReplyThumbnail = Option<bool>{
	"nagram.hideReplyThumbnail", Scope::Device, false,
	Category::Interface, "lng_nagram_hide_reply_thumbnail",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kIgnoreChatTheme = Option<bool>{
	"nagram.ignoreChatTheme", Scope::Device, false,
	Category::Interface, "lng_nagram_ignore_chat_theme" };
inline constexpr auto kIgnorePrivateChatTheme = Option<bool>{
	"nagram.ignorePrivateChatTheme", Scope::Device, false,
	Category::Interface, "lng_nagram_ignore_private_chat_theme" };
inline constexpr auto kIgnoreChannelChatTheme = Option<bool>{
	"nagram.ignoreChannelChatTheme", Scope::Device, false,
	Category::Interface, "lng_nagram_ignore_channel_chat_theme" };
inline constexpr auto kAccountNameInTitle = Option<bool>{
	"nagram.accountNameInTitle", Scope::Device, false,
	Category::Interface, "lng_nagram_account_name_in_title" };
inline constexpr auto kAlwaysSeasonal = Option<bool>{
	"nagram.alwaysSeasonalDecorations", Scope::Device, false,
	Category::Interface, "lng_nagram_always_seasonal", kRestart };
inline const auto kMainMenuConfig = Option<QByteArray>{
	"nagram.mainMenu", Scope::Device, QByteArray(),
	Category::Interface, "lng_nagram_main_menu", 0,
	ValidMainMenuBytes };
inline const auto kAppIcon = Option<QString>{
	"nagram.appIcon", Scope::Device, QString(),
	Category::Interface, "lng_nagram_app_icon",
	static_cast<unsigned>(Flag::LocalOnly), ValidAppIcon };
inline constexpr auto kHideAppIconBadge = Option<bool>{
	"nagram.hideAppIconBadge", Scope::Device, false,
	Category::Interface, "lng_nagram_hide_app_icon_badge" };
inline constexpr auto kNotificationDelay = Option<int>{
	"nagram.notificationDelay", Scope::Device, 0,
	Category::Interface, "lng_nagram_notification_delay", 0,
	[](const int &value) { return value == -1 || value == 0 || value == 500
		|| value == 1000 || value == 2000 || value == 5000
		|| value == 10000 || value == 30000 || value == 60000; } };
inline constexpr auto kOtherDeviceNotificationDelay = Option<int>{
	"nagram.otherDeviceNotificationDelay", Scope::Device, 0,
	Category::Interface, "lng_nagram_other_device_notification_delay", 0,
	kNotificationDelay.validate };
inline constexpr auto kHalfwidthUiPunctuation = Option<bool>{
	"nagram.halfwidthUiPunctuation", Scope::Device, false,
	Category::Interface, "lng_nagram_halfwidth_ui_punctuation", kRestart };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kBubbleRoundness));
	Expects(registry.Add(kAvatarRoundness));
	Expects(registry.Add(kUniformAvatarShapes));
	Expects(registry.Add(kTextMessageWidth));
	Expects(registry.Add(kWideChannelPosts));
	Expects(registry.Add(kHideBubbleTail));
	Expects(registry.Add(kThemeReplyColors));
	Expects(registry.Add(kHideReplyThumbnail));
	Expects(registry.Add(kIgnoreChatTheme));
	Expects(registry.Add(kIgnorePrivateChatTheme));
	Expects(registry.Add(kIgnoreChannelChatTheme));
	Expects(registry.Add(kAccountNameInTitle));
	Expects(registry.Add(kAlwaysSeasonal));
	Expects(registry.Add(kMainMenuConfig));
	Expects(registry.Add(kAppIcon));
	Expects(registry.Add(kHideAppIconBadge));
	Expects(registry.Add(kNotificationDelay));
	Expects(registry.Add(kOtherDeviceNotificationDelay));
	Expects(registry.Add(kHalfwidthUiPunctuation));
}

} // namespace Nagram::Interface
