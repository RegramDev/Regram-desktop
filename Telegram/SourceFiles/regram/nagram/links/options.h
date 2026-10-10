#pragma once

#include "regram/nagram/core/options.h"

#include <QtCore/QSize>

#include <algorithm>

namespace Nagram::Links {

enum class HashtagPage { Follow, ThisChat, MyMessages };

inline constexpr auto kDisableOfficialAutoLogin = Option<bool>{
	"nagram.disableOfficialAutoLogin", Scope::Device, false,
	Category::Rules, "lng_nagram_disable_official_auto_login" };
inline constexpr auto kHashtagSearchPageChannel = Option<int>{
	"nagram.hashtagSearchPageChannel", Scope::Device, 0,
	Category::Rules, "lng_nagram_hashtag_page_channel", 0,
	[](const int &value) { return value >= 0 && value <= 2; } };
inline constexpr auto kHashtagSearchPageChat = Option<int>{
	"nagram.hashtagSearchPageChat", Scope::Device, 0,
	Category::Rules, "lng_nagram_hashtag_page_chat", 0,
	[](const int &value) { return value >= 0 && value <= 2; } };

[[nodiscard]] constexpr bool ValidWebAppScale(const int &value) {
	return value >= 100 && value <= 200 && value % 25 == 0;
}

inline constexpr auto kWebAppWidthScale = Option<int>{
	"nagram.webAppWidthScale", Scope::Device, 100,
	Category::Rules, "lng_nagram_web_app_width",
	static_cast<unsigned>(Flag::LocalOnly), ValidWebAppScale };
inline constexpr auto kWebAppHeightScale = Option<int>{
	"nagram.webAppHeightScale", Scope::Device, 100,
	Category::Rules, "lng_nagram_web_app_height",
	static_cast<unsigned>(Flag::LocalOnly), ValidWebAppScale };

inline constexpr auto kWebAppAndroidPlatform = Option<bool>{
	"nagram.webAppAndroidPlatform", Scope::Device, false,
	Category::Rules, "lng_nagram_web_app_android_platform" };

[[nodiscard]] constexpr const char *WebAppPlatformName(bool android) {
	return android ? "android" : "tdesktop";
}

[[nodiscard]] inline QSize ScaledPanelSize(
		QSize base,
		int widthScale,
		int heightScale,
		QSize available) {
	auto result = QSize(
		base.width() * widthScale / 100,
		base.height() * heightScale / 100);
	if (!available.isEmpty()) {
		result = result.boundedTo(available.expandedTo(base));
	}
	return result;
}

[[nodiscard]] inline HashtagPage ResolveHashtagPage(
		bool clicked,
		bool broadcast,
		int channelValue,
		int chatValue) {
	const auto value = broadcast ? channelValue : chatValue;
	return !clicked
		? HashtagPage::Follow
		: (value == 1)
		? HashtagPage::ThisChat
		: (value == 2)
		? HashtagPage::MyMessages
		: HashtagPage::Follow;
}

inline void RegisterBehaviorOptions(Registry &registry) {
	Expects(registry.Add(kDisableOfficialAutoLogin));
	Expects(registry.Add(kHashtagSearchPageChannel));
	Expects(registry.Add(kHashtagSearchPageChat));
	Expects(registry.Add(kWebAppWidthScale));
	Expects(registry.Add(kWebAppHeightScale));
	Expects(registry.Add(kWebAppAndroidPlatform));
}

} // namespace Nagram::Links
