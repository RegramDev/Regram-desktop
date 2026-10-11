#pragma once

#include "regram/nagram/core/options.h"

namespace Nagram::Media {

inline constexpr auto kStickerScale = Option<int>{
	"nagram.stickerScale", Scope::Device, 100,
	Category::Media, "lng_nagram_sticker_scale",
	static_cast<unsigned>(Flag::RefreshMessageView),
	[](const int &value) {
		return value >= 50 && value <= 200 && value % 25 == 0;
	} };
inline constexpr auto kHideStickerTime = Option<bool>{
	"nagram.hideStickerTime", Scope::Device, false,
	Category::Media, "lng_nagram_hide_sticker_time",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kRecentStickerLimit = Option<int>{
	"nagram.recentStickerLimit", Scope::Device, 0,
	Category::Media, "lng_nagram_recent_sticker_limit", 0,
	[](const int &value) { return value >= 0 && value <= 200; } };
inline constexpr auto kHideGroupStickers = Option<bool>{
	"nagram.hideGroupStickers", Scope::Device, false,
	Category::Media, "lng_nagram_hide_group_stickers" };
inline constexpr auto kHideRecommendedStickers = Option<bool>{
	"nagram.hideRecommendedStickers", Scope::Device, false,
	Category::Media, "lng_nagram_hide_recommended_stickers" };
inline constexpr auto kHideRecommendedEmoji = Option<bool>{
	"nagram.hideRecommendedEmoji", Scope::Device, false,
	Category::Media, "lng_nagram_hide_recommended_emoji" };
inline constexpr auto kHideGifCategories = Option<bool>{
	"nagram.hideGifCategories", Scope::Device, false,
	Category::Media, "lng_nagram_hide_gif_categories" };
inline constexpr auto kHideGreetingSticker = Option<bool>{
	"nagram.hideGreetingSticker", Scope::Device, false,
	Category::Media, "lng_nagram_hide_greeting_sticker" };
inline constexpr auto kDisableVideoAutoplay = Option<bool>{
	"nagram.disableVideoAutoplay", Scope::Device, false,
	Category::Media, "lng_nagram_disable_video_autoplay",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kGifPlaybackControls = Option<bool>{
	"nagram.gifPlaybackControls", Scope::Device, false,
	Category::Media, "lng_nagram_gif_playback_controls" };
inline constexpr auto kMp4FilePreview = Option<bool>{
	"nagram.mp4FilePreview", Scope::Device, false,
	Category::Media, "lng_nagram_mp4_file_preview" };

inline constexpr auto kSmallGifs = Option<bool>{
	"nagram.smallGifs", Scope::Device, false,
	Category::Media, "lng_nagram_small_gifs",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kStickerPanelScale = Option<int>{
	"nagram.stickerPanelScale", Scope::Device, 100,
	Category::Media, "lng_nagram_sticker_panel_scale",
	static_cast<unsigned>(Flag::RequiresRestart),
	[](const int &value) {
		return value >= 50 && value <= 200 && value % 25 == 0;
	} };
inline constexpr auto kDownloadsInMainMenu = Option<bool>{
	"nagram.downloadsInMainMenu", Scope::Device, false,
	Category::Media, "lng_nagram_downloads_in_main_menu" };
inline constexpr auto kBlockExecutableAutoDownload = Option<bool>{
	"nagram.blockExecutableAutoDownload", Scope::Device, false,
	Category::Media, "lng_nagram_block_executable_auto_download" };
inline constexpr auto kBlockArchiveAutoDownload = Option<bool>{
	"nagram.blockArchiveAutoDownload", Scope::Device, false,
	Category::Media, "lng_nagram_block_archive_auto_download" };

inline constexpr auto kLoadingExperiment = Option<bool>{
	"nagram.regramMediaLoadingExperiment", Scope::Device, false,
	Category::Media, "lng_regram_media_loading_experiment" };
inline constexpr auto kDefaultVideoQuality = Option<int>{
	"nagram.regramDefaultVideoQuality", Scope::Device, 0,
	Category::Media, "lng_regram_default_video_quality", 0,
	[](const int &value) { return value >= 0 && value <= 2; } };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kStickerScale));
	Expects(registry.Add(kLoadingExperiment));
	Expects(registry.Add(kDefaultVideoQuality));
	Expects(registry.Add(kHideStickerTime));
	Expects(registry.Add(kRecentStickerLimit));
	Expects(registry.Add(kHideGroupStickers));
	Expects(registry.Add(kHideRecommendedStickers));
	Expects(registry.Add(kHideRecommendedEmoji));
	Expects(registry.Add(kHideGifCategories));
	Expects(registry.Add(kHideGreetingSticker));
	Expects(registry.Add(kDisableVideoAutoplay));
	Expects(registry.Add(kGifPlaybackControls));
	Expects(registry.Add(kMp4FilePreview));
	Expects(registry.Add(kSmallGifs));
	Expects(registry.Add(kStickerPanelScale));
	Expects(registry.Add(kDownloadsInMainMenu));
	Expects(registry.Add(kBlockExecutableAutoDownload));
	Expects(registry.Add(kBlockArchiveAutoDownload));
}

} // namespace Nagram::Media
