#pragma once

#include "regram/nagram/core/options.h"

namespace Nagram::Chats {

inline constexpr auto kRefreshDialogList = static_cast<unsigned>(
	Flag::RefreshDialogList);

inline constexpr auto kCompactList = Option<bool>{
	"nagram.chatListCompact", Scope::Device, false,
	Category::Chats, "lng_nagram_compact_chat_list", kRefreshDialogList };
inline constexpr auto kPreviewLines = Option<int>{
	"nagram.chatPreviewLines", Scope::Device, 0,
	Category::Chats, "lng_nagram_chat_preview_lines", kRefreshDialogList,
	[](const int &value) { return value >= 0 && value <= 3; } };
inline constexpr auto kHideSavedAndArchivedPreviews = Option<bool>{
	"nagram.hideSavedAndArchivedPreviews", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_saved_and_archived_previews",
	kRefreshDialogList };
inline constexpr auto kHideStories = Option<bool>{
	"nagram.hideStories", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_stories" };
inline constexpr auto kHideAllChatsFolder = Option<bool>{
	"nagram.hideAllChatsFolder", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_all_chats_folder" };
inline constexpr auto kShowArchiveInFolders = Option<bool>{
	"nagram.showArchiveInFolders", Scope::Device, false,
	Category::Chats, "lng_nagram_show_archive_in_folders",
	kRefreshDialogList };
inline constexpr auto kArchiveInFolderList = Option<bool>{
	"nagram.archiveInFolderList", Scope::Device, false,
	Category::Chats, "lng_nagram_archive_in_folder_list" };
inline constexpr auto kSavedInFolderList = Option<bool>{
	"nagram.savedInFolderList", Scope::Device, false,
	Category::Chats, "lng_nagram_saved_in_folder_list" };
inline constexpr auto kHideFolderUnreadCounters = Option<bool>{
	"nagram.hideFolderUnreadCounters", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_folder_unread_counters" };
inline constexpr auto kStartupFolderMode = Option<int>{
	"nagram.startupFolderMode", Scope::Account, 0,
	Category::Chats, "lng_nagram_startup_folder", 0,
	[](const int &value) { return value >= 0 && value <= 2; } };
inline constexpr auto kStartupFolderId = Option<int>{
	"nagram.startupFolderId", Scope::Account, 0,
	Category::Chats, "lng_nagram_startup_folder", 0,
	[](const int &value) { return value >= 0; } };
inline constexpr auto kLastOpenedFolderId = Option<int>{
	"nagram.lastOpenedFolderId", Scope::Account, 0,
	Category::Chats, "lng_nagram_startup_folder", 0,
	[](const int &value) { return value >= 0; } };
inline constexpr auto kChatSort = Option<int>{
	"nagram.chatSort", Scope::Device, 0,
	Category::Chats, "lng_nagram_chat_sort", 0,
	[](const int &value) {
		if (value == 0) return true;
		if (value < 0 || value > 0xFFF) return false;
		auto seen = 0;
		for (auto i = 0; i != 4; ++i) {
			seen |= 1 << ((value >> (4 + i * 2)) & 3);
		}
		return seen == 0xF;
	} };
[[nodiscard]] inline bool ValidFolderIds(const QString &value) {
		if (value.isEmpty()) return true;
		auto previous = 0;
		for (const auto &part : value.split(u',')) {
			auto valid = false;
			const auto id = part.toInt(&valid);
			if (!valid || id <= previous || part != QString::number(id)) {
				return false;
			}
			previous = id;
		}
		return true;
}
inline const auto kManagedFolderIds = Option<QString>{
	"nagram.managedFolderIds", Scope::Account, QString(),
	Category::Chats, "lng_nagram_managed_only", 0, ValidFolderIds };
inline const auto kRecentFolderIds = Option<QString>{
	"nagram.recentFolderIds", Scope::Account, QString(),
	Category::Chats, "lng_nagram_recent_chats",
	static_cast<unsigned>(Flag::Hidden), ValidFolderIds };
inline constexpr auto kHideSponsoredMessages = Option<bool>{
	"nagram.hideSponsoredMessages", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_sponsored_messages" };
inline constexpr auto kHideProxySponsor = Option<bool>{
	"nagram.hideProxySponsor", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_proxy_sponsor" };
inline constexpr auto kHidePremiumPromotions = Option<bool>{
	"nagram.hidePremiumPromotions", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_premium_promotions" };
inline constexpr auto kHideBirthdaySuggestions = Option<bool>{
	"nagram.hideBirthdaySuggestions", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_birthday_suggestions" };
inline constexpr auto kDisableCommunityGrouping = Option<bool>{
	"nagram.disableCommunityGrouping", Scope::Device, false,
	Category::Chats, "lng_nagram_disable_community_grouping",
	static_cast<unsigned>(Flag::RequiresRestart) };
inline constexpr auto kCompactFolderTabs = Option<bool>{
	"nagram.compactFolderTabs", Scope::Device, false,
	Category::Chats, "lng_nagram_compact_folder_tabs",
	static_cast<unsigned>(Flag::RequiresRestart) };
inline constexpr auto kDisableGlobalSearch = Option<bool>{
	"nagram.disableGlobalSearch", Scope::Device, false,
	Category::Chats, "lng_nagram_disable_global_search" };
inline constexpr auto kChooseFolderAfterJoin = Option<bool>{
	"nagram.chooseFolderAfterJoin", Scope::Device, false,
	Category::Chats, "lng_nagram_choose_folder_after_join" };
inline constexpr auto kHidePhoneSuggestion = Option<bool>{
	"nagram.hidePhoneSuggestion", Scope::Device, false,
	Category::Chats, "lng_nagram_hide_phone_suggestion" };
inline constexpr auto kDisableScrollToNextChannel = Option<bool>{
	"nagram.disableScrollToNextChannel", Scope::Device, false,
	Category::Chats, "lng_nagram_disable_scroll_to_next_channel" };
inline constexpr auto kDisableScrollToNextTopic = Option<bool>{
	"nagram.disableScrollToNextTopic", Scope::Device, false,
	Category::Chats, "lng_nagram_disable_scroll_to_next_topic" };

inline constexpr auto kMinimumRecentChats = 5;
inline constexpr auto kMaximumRecentChats = 100;

[[nodiscard]] inline bool ValidRecentChats(const QString &value) {
	if (value.isEmpty()) {
		return true;
	}
	const auto parts = value.split(u',');
	if (parts.size() > kMaximumRecentChats) {
		return false;
	}
	for (const auto &part : parts) {
		auto ok = false;
		if (!part.toULongLong(&ok) || !ok) {
			return false;
		}
	}
	return true;
}

inline constexpr auto kRecentChats = Option<bool>{
	"nagram.recentChats", Scope::Device, false,
	Category::Chats, "lng_nagram_recent_chats_option" };
inline constexpr auto kRecentChatsLimit = Option<int>{
	"nagram.recentChatsLimit", Scope::Device, 30,
	Category::Chats, "lng_nagram_recent_chats_limit", 0,
	[](const int &value) {
		return value >= kMinimumRecentChats && value <= kMaximumRecentChats;
	} };
inline const auto kRecentChatsList = Option<QString>{
	"nagram.recentChatsList", Scope::Account, QString(),
	Category::Chats, "lng_nagram_recent_chats",
	static_cast<unsigned>(Flag::Hidden), ValidRecentChats };

inline constexpr auto kMaximumReadingPositions = 100;

[[nodiscard]] inline bool ValidReadingPositions(const QString &value) {
	if (value.isEmpty()) {
		return true;
	}
	const auto entries = value.split(u',');
	if (entries.size() > kMaximumReadingPositions) {
		return false;
	}
	for (const auto &entry : entries) {
		const auto parts = entry.split(u':');
		auto peerOk = false;
		auto msgOk = false;
		if (parts.size() != 2
			|| !parts[0].toULongLong(&peerOk) || !peerOk
			|| parts[1].toLongLong(&msgOk) <= 0 || !msgOk) {
			return false;
		}
	}
	return true;
}

inline constexpr auto kChatTools = Option<bool>{
	"nagram.chatTools", Scope::Device, false,
	Category::Chats, "lng_nagram_chat_tools" };
inline constexpr auto kRecentInShare = Option<bool>{
	"nagram.recentChatsInShare", Scope::Device, false,
	Category::Chats, "lng_nagram_recent_in_share" };
inline constexpr auto kSaveReadingPosition = Option<bool>{
	"nagram.saveReadingPosition", Scope::Device, false,
	Category::Chats, "lng_nagram_save_reading_position" };
inline const auto kReadingPositions = Option<QString>{
	"nagram.readingPositions", Scope::Account, QString(),
	Category::Chats, "lng_nagram_save_reading_position",
	static_cast<unsigned>(Flag::Hidden), ValidReadingPositions };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kCompactList));
	Expects(registry.Add(kPreviewLines));
	Expects(registry.Add(kHideSavedAndArchivedPreviews));
	Expects(registry.Add(kHideStories));
	Expects(registry.Add(kHideAllChatsFolder));
	Expects(registry.Add(kShowArchiveInFolders));
	Expects(registry.Add(kArchiveInFolderList));
	Expects(registry.Add(kSavedInFolderList));
	Expects(registry.Add(kHideFolderUnreadCounters));
	Expects(registry.Add(kStartupFolderMode));
	Expects(registry.Add(kStartupFolderId));
	Expects(registry.Add(kLastOpenedFolderId));
	Expects(registry.Add(kChatSort));
	Expects(registry.Add(kManagedFolderIds));
	Expects(registry.Add(kHideSponsoredMessages));
	Expects(registry.Add(kHideProxySponsor));
	Expects(registry.Add(kHidePremiumPromotions));
	Expects(registry.Add(kHideBirthdaySuggestions));
	Expects(registry.Add(kHidePhoneSuggestion));
	Expects(registry.Add(kDisableCommunityGrouping));
	Expects(registry.Add(kCompactFolderTabs));
	Expects(registry.Add(kDisableGlobalSearch));
	Expects(registry.Add(kChooseFolderAfterJoin));
	Expects(registry.Add(kDisableScrollToNextChannel));
	Expects(registry.Add(kDisableScrollToNextTopic));
	Expects(registry.Add(kRecentChats));
	Expects(registry.Add(kRecentChatsLimit));
	Expects(registry.Add(kRecentChatsList));
	Expects(registry.Add(kRecentInShare));
	Expects(registry.Add(kRecentFolderIds));
	Expects(registry.Add(kChatTools));
	Expects(registry.Add(kSaveReadingPosition));
	Expects(registry.Add(kReadingPositions));
}

} // namespace Nagram::Chats
