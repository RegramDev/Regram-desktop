#pragma once

#include "regram/nagram/core/options.h"

#include <array>
#include <optional>
#include <vector>

namespace Nagram::Menu {

enum class ActionId : int {
	Reply = 1,
	Edit,
	Copy,
	CopyLink,
	Forward,
	Translate,
	Pin,
	Select,
	Statistics,
	Report,
	BlockSender,
	Image,
	Delete,
	StickerPack,
	Repeat,
	RepeatAsCopy,
	ForwardWithoutQuote,
	Batch,
	SelectSender,
	MediaInfo,
	Screenshot = 21,
	Reading = 22,
	FilterAuthor = 23,
	DeleteDownload = 25,
	QuickRating = 26,
	SelectBetween = 27,
	SeenBy = 28,
	MessageDetails = 29,
	SaveToSaved = 31,
	SelectAll = 32,
	Summarize = 36,
	TranscribeSelected = 37,
	CopyMarkdown = 39,
};

enum class Visibility { Show, Hide, WithOption };

struct Entry {
	ActionId id;
	const char *titleKey;
};

struct Slot {
	std::optional<ActionId> id;
	bool separator = false;
};

inline constexpr auto kEntries = std::array<Entry, 33>({{
	{ ActionId::Reply, "lng_nagram_menu_reply" },
	{ ActionId::Edit, "lng_nagram_menu_edit" },
	{ ActionId::Copy, "lng_nagram_menu_copy" },
	{ ActionId::CopyLink, "lng_nagram_menu_copy_link" },
	{ ActionId::Forward, "lng_nagram_menu_forward" },
	{ ActionId::Translate, "lng_nagram_menu_translate" },
	{ ActionId::Pin, "lng_nagram_menu_pin" },
	{ ActionId::Select, "lng_nagram_menu_select" },
	{ ActionId::Statistics, "lng_nagram_menu_statistics" },
	{ ActionId::Report, "lng_nagram_menu_report" },
	{ ActionId::BlockSender, "lng_nagram_menu_block_sender" },
	{ ActionId::Image, "lng_nagram_menu_image" },
	{ ActionId::Delete, "lng_nagram_menu_delete" },
	{ ActionId::StickerPack, "lng_nagram_menu_sticker_pack" },
	{ ActionId::Repeat, "lng_nagram_menu_repeat" },
	{ ActionId::RepeatAsCopy, "lng_nagram_menu_repeat_as_copy" },
	{ ActionId::ForwardWithoutQuote, "lng_nagram_menu_forward_without_quote" },
	{ ActionId::Batch, "lng_nagram_menu_batch" },
	{ ActionId::SelectSender, "lng_nagram_menu_select_sender" },
	{ ActionId::MediaInfo, "lng_nagram_menu_media_info" },
	{ ActionId::Screenshot, "lng_nagram_menu_screenshot" },
	{ ActionId::Reading, "lng_nagram_menu_reading" },
	{ ActionId::FilterAuthor, "lng_nagram_filter_author_hide" },
	{ ActionId::DeleteDownload, "lng_nagram_menu_delete_download" },
	{ ActionId::QuickRating, "lng_nagram_menu_quick_rating" },
	{ ActionId::SelectBetween, "lng_nagram_menu_select_between" },
	{ ActionId::SeenBy, "lng_nagram_menu_seen_by" },
	{ ActionId::MessageDetails, "lng_nagram_menu_details" },
	{ ActionId::SaveToSaved, "lng_nagram_menu_save_to_saved" },
	{ ActionId::SelectAll, "lng_nagram_menu_select_all" },
	{ ActionId::Summarize, "lng_nagram_menu_summarize" },
	{ ActionId::TranscribeSelected, "lng_nagram_menu_transcribe_selected" },
	{ ActionId::CopyMarkdown, "lng_nagram_menu_copy_markdown" },
}});

[[nodiscard]] bool IsUpstream(ActionId id);
[[nodiscard]] Visibility DefaultVisibility(ActionId id);
[[nodiscard]] bool ValidateConfig(const QByteArray &raw);
[[nodiscard]] Visibility ReadVisibility(const QByteArray &raw, ActionId id);
[[nodiscard]] QByteArray WriteVisibility(
	const QByteArray &raw,
	ActionId id,
	Visibility visibility);
[[nodiscard]] bool Visible(Visibility visibility, bool optionHeld);
[[nodiscard]] int EndPosition(const std::vector<Slot> &slots);

inline const auto kMenuConfig = Option<QByteArray>{
	"nagram.messageMenu", Scope::Device, QByteArray(),
	Category::Menu, "lng_nagram_menu", static_cast<unsigned>(Flag::Exportable),
	ValidateConfig };
[[nodiscard]] inline bool ValidQuickRating(const QString &value) {
	return value.size() <= 64;
}

inline const auto kQuickRatingFirst = Option<QString>{
	"nagram.quickRatingFirst", Scope::Device, QString(),
	Category::Menu, "lng_nagram_menu_quick_rating_first",
	static_cast<unsigned>(Flag::Exportable), ValidQuickRating };
inline const auto kQuickRatingSecond = Option<QString>{
	"nagram.quickRatingSecond", Scope::Device, QString(),
	Category::Menu, "lng_nagram_menu_quick_rating_second",
	static_cast<unsigned>(Flag::Exportable), ValidQuickRating };
inline constexpr auto kConfirmRepeat = Option<bool>{
	"nagram.confirmRepeat", Scope::Device, false,
	Category::Menu, "lng_nagram_menu_confirm_repeat",
	static_cast<unsigned>(Flag::Exportable) };

inline constexpr auto kCompactMenu = Option<bool>{
	"nagram.compactMessageMenu", Scope::Device, false,
	Category::Menu, "lng_nagram_menu_compact",
	static_cast<unsigned>(Flag::Exportable) };
inline constexpr auto kRepeatWithoutQuote = Option<bool>{
	"nagram.repeatWithoutQuote", Scope::Device, false,
	Category::Menu, "lng_nagram_menu_repeat_without_quote",
	static_cast<unsigned>(Flag::Exportable) };
inline constexpr auto kNoRepeatInChannels = Option<bool>{
	"nagram.noRepeatInChannels", Scope::Device, false,
	Category::Menu, "lng_nagram_menu_no_repeat_channels",
	static_cast<unsigned>(Flag::Exportable) };
inline constexpr auto kScrollAfterRepeat = Option<bool>{
	"nagram.scrollAfterRepeat", Scope::Device, false,
	Category::Menu, "lng_nagram_menu_scroll_after_repeat",
	static_cast<unsigned>(Flag::Exportable) };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kMenuConfig));
	Expects(registry.Add(kConfirmRepeat));
	Expects(registry.Add(kQuickRatingFirst));
	Expects(registry.Add(kQuickRatingSecond));
	Expects(registry.Add(kCompactMenu));
	Expects(registry.Add(kRepeatWithoutQuote));
	Expects(registry.Add(kNoRepeatInChannels));
	Expects(registry.Add(kScrollAfterRepeat));
}

} // namespace Nagram::Menu
