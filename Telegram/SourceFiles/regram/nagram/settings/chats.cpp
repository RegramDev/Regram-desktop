#include "regram/nagram/settings/chats.h"
#include "regram/ayu/ui/settings/settings_chats.h"

#include "regram/nagram/chats/options.h"
#include "regram/nagram/chats/cleanup.h"
#include "regram/nagram/chats/local_pins.h"
#include "regram/nagram/chats/local_pins_model.h"
#include "regram/nagram/chats/sort.h"
#include "regram/nagram/core/options.h"
#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/restart.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"

#include <bit>

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class ChatsSection final : public Section<ChatsSection> {
public:
	ChatsSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_chats();
	}

	static const SectionBuildMethod kBuild;
};

void AddToggle(
		SectionBuilder &builder,
		const Option<bool> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	const auto controller = builder.controller();
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(option),
		.keywords = std::move(keywords),
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([option, controller](bool value) {
			Expects(ForDevice().Set(option, value));
			if (option.flags & static_cast<unsigned>(Flag::RequiresRestart)) {
				ShowRestartPrompt(controller);
			}
		}, button->lifetime());
	}
}

QString PreviewLinesLabel(int value) {
	switch (value) {
	case 1: return tr::lng_nagram_preview_one(tr::now);
	case 2: return tr::lng_nagram_preview_two(tr::now);
	case 3: return tr::lng_nagram_preview_three(tr::now);
	default: return tr::lng_nagram_preview_follow(tr::now);
	}
}

void PreviewLinesBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_chat_preview_lines());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Chats::kPreviewLines));
	for (auto value = 0; value != 4; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, PreviewLinesLabel(value), st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Chats::kPreviewLines, value));
		box->closeBox();
	});
}

QString StartupFolderLabel(not_null<Main::Session*> session) {
	auto &options = ForAccount(session);
	switch (options.Get(Chats::kStartupFolderMode)) {
	case 1: return tr::lng_nagram_startup_folder_last(tr::now);
	case 2: {
		const auto id = options.Get(Chats::kStartupFolderId);
		const auto &list = session->data().chatsFilters().list();
		const auto found = ranges::find(list, id, &Data::ChatFilter::id);
		return found == list.end()
			? tr::lng_nagram_preview_follow(tr::now)
			: found->titleText().text;
	}
	default: return tr::lng_nagram_preview_follow(tr::now);
	}
}

void StartupFolderBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_startup_folder());
	const auto options = &ForAccount(session);
	const auto mode = options->Get(Chats::kStartupFolderMode);
	const auto current = mode == 2
		? options->Get(Chats::kStartupFolderId) + 2
		: mode;
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(current);
	for (const auto &value : { 0, 1 }) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value,
			value ? tr::lng_nagram_startup_folder_last(tr::now)
				: tr::lng_nagram_preview_follow(tr::now),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	for (const auto &filter : session->data().chatsFilters().list()) {
		if (!filter.id()) continue;
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, filter.id() + 2,
			tr::lng_nagram_startup_folder_specific(tr::now)
				+ u" · "_q + filter.titleText().text,
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (value > 2) {
			Expects(options->Set(Chats::kStartupFolderId, value - 2));
			Expects(options->Set(Chats::kStartupFolderMode, 2));
		} else {
			Expects(options->Set(Chats::kStartupFolderMode, value));
		}
		box->closeBox();
	});
}

void RecentChatsLimitBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_recent_chats_limit());
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_recent_chats_limit_hint(),
		QString::number(ForDevice().Get(Chats::kRecentChatsLimit))));
	field->setInputMethodHints(Qt::ImhDigitsOnly);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		auto valid = false;
		const auto value = field->getLastText().trimmed().toInt(&valid);
		if (!valid || !ForDevice().Set(Chats::kRecentChatsLimit, value)) {
			field->showError();
			return;
		}
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = ChatsSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_chats,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/list"_q,
		.title = tr::lng_nagram_chat_list(),
		.keywords = { u"list"_q, u"layout"_q },
	});
	AddToggle(builder, Chats::kCompactList,
		tr::lng_nagram_compact_chat_list(),
		u"regram/nagram/chats/compact"_q,
		{ u"compact"_q, u"list"_q });
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/chats/preview-lines"_q,
		.title = tr::lng_nagram_chat_preview_lines(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Chats::kPreviewLines)
			| rpl::map(PreviewLinesLabel),
		.onClick = [=] { controller->show(Box(PreviewLinesBox)); },
		.keywords = { u"preview"_q, u"lines"_q },
	});
	AddToggle(builder, Chats::kHideSavedAndArchivedPreviews,
		tr::lng_nagram_hide_saved_and_archived_previews(),
		u"regram/nagram/chats/hide-special-previews"_q,
		{ u"saved"_q, u"archive"_q, u"preview"_q });
	AddToggle(builder, Chats::kHideStories,
		tr::lng_nagram_hide_stories(),
		u"regram/nagram/chats/hide-stories"_q,
		{ u"stories"_q });
	builder.addButton({
		.id = u"regram/nagram/chats/cleanup"_q,
		.title = tr::lng_nagram_cleanup(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { Chats::ShowCleanup(controller); },
		.keywords = { u"clean"_q, u"archive"_q, u"delete"_q, u"inactive"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/folders"_q,
		.title = tr::lng_nagram_folders(),
		.keywords = { u"folders"_q },
	});
	const auto session = builder.session();
	builder.addButton({
		.id = u"regram/nagram/chats/startup-folder"_q,
		.title = tr::lng_nagram_startup_folder(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(StartupFolderLabel(session)) | rpl::then(
			ForAccount(session).changes() | rpl::map([=](auto) {
				return StartupFolderLabel(session);
			})),
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				StartupFolderBox(box, session);
			}));
		},
		.keywords = { u"startup"_q, u"folder"_q },
	});
	AddToggle(builder, Chats::kHideAllChatsFolder,
		tr::lng_nagram_hide_all_chats_folder(),
		u"regram/nagram/chats/hide-all"_q,
		{ u"all chats"_q, u"folders"_q });
	AddToggle(builder, Chats::kShowArchiveInFolders,
		tr::lng_nagram_show_archive_in_folders(),
		u"regram/nagram/chats/archive-in-folders"_q,
		{ u"archive"_q, u"folders"_q });
	AddToggle(builder, Chats::kArchiveInFolderList,
		tr::lng_nagram_archive_in_folder_list(),
		u"regram/nagram/chats/archive-in-folder-list"_q,
		{ u"archive"_q, u"folders"_q });
	AddToggle(builder, Chats::kSavedInFolderList,
		tr::lng_nagram_saved_in_folder_list(),
		u"regram/nagram/chats/saved-in-folder-list"_q,
		{ u"saved messages"_q, u"folders"_q });
	AddToggle(builder, Chats::kHideFolderUnreadCounters,
		tr::lng_nagram_hide_folder_unread_counters(),
		u"regram/nagram/chats/hide-folder-unread"_q,
		{ u"unread"_q, u"folders"_q });
	AddToggle(builder, Chats::kCompactFolderTabs,
		tr::lng_nagram_compact_folder_tabs(),
		u"regram/nagram/chats/compact-folder-tabs"_q,
		{ u"folders"_q, u"tabs"_q, u"compact"_q });
	AddToggle(builder, Chats::kChooseFolderAfterJoin,
		tr::lng_nagram_choose_folder_after_join(),
		u"regram/nagram/chats/choose-folder-after-join"_q,
		{ u"join"_q, u"folder"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/sorting"_q,
		.title = tr::lng_nagram_sorting(),
		.keywords = { u"sort"_q, u"order"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/chats/chat-sort"_q,
		.title = tr::lng_nagram_chat_sort(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Chats::kChatSort) | rpl::map([](int value) {
			const auto count = std::popcount(unsigned(value & 15));
			return count
				? QString::number(count) + tr::lng_nagram_sort_active_suffix(tr::now)
				: tr::lng_nagram_preview_follow(tr::now);
		}),
		.onClick = [=] { controller->show(Box(Chats::ChatSortBox)); },
		.keywords = { u"sort"_q, u"unread"_q, u"contacts"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/promotions"_q,
		.title = tr::lng_nagram_promotions(),
		.keywords = { u"promotions"_q, u"ads"_q },
	});
	AddToggle(builder, Chats::kHideSponsoredMessages,
		tr::lng_nagram_hide_sponsored_messages(),
		u"regram/nagram/chats/hide-sponsored"_q,
		{ u"sponsored"_q, u"search ads"_q });
	AddToggle(builder, Chats::kHideProxySponsor,
		tr::lng_nagram_hide_proxy_sponsor(),
		u"regram/nagram/chats/hide-proxy-sponsor"_q,
		{ u"proxy"_q, u"sponsored channel"_q });
	AddToggle(builder, Chats::kHidePremiumPromotions,
		tr::lng_nagram_hide_premium_promotions(),
		u"regram/nagram/chats/hide-premium-promotions"_q,
		{ u"Premium"_q, u"promotions"_q });
	AddToggle(builder, Chats::kHideBirthdaySuggestions,
		tr::lng_nagram_hide_birthday_suggestions(),
		u"regram/nagram/chats/hide-birthday"_q,
		{ u"birthday"_q, u"suggestion"_q });
	AddToggle(builder, Chats::kHidePhoneSuggestion,
		tr::lng_nagram_hide_phone_suggestion(),
		u"regram/nagram/chats/hide-phone-suggestion"_q,
		{ u"phone number"_q, u"suggestion"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/scroll-navigation"_q,
		.title = tr::lng_nagram_scroll_navigation(),
		.keywords = { u"scroll"_q, u"navigation"_q },
	});
	AddToggle(builder, Chats::kDisableScrollToNextChannel,
		tr::lng_nagram_disable_scroll_to_next_channel(),
		u"regram/nagram/chats/disable-next-channel"_q,
		{ u"scroll"_q, u"channel"_q });
	AddToggle(builder, Chats::kDisableScrollToNextTopic,
		tr::lng_nagram_disable_scroll_to_next_topic(),
		u"regram/nagram/chats/disable-next-topic"_q,
		{ u"scroll"_q, u"topic"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/navigation"_q,
		.title = tr::lng_nagram_navigation(),
		.keywords = { u"navigation"_q, u"recent"_q },
	});
	AddToggle(builder, Chats::kDisableGlobalSearch,
		tr::lng_nagram_disable_global_search(),
		u"regram/nagram/chats/disable-global-search"_q,
		{ u"search"_q, u"global"_q, u"public"_q });
	AddToggle(builder, Chats::kDisableCommunityGrouping,
		tr::lng_nagram_disable_community_grouping(),
		u"regram/nagram/chats/disable-community-grouping"_q,
		{ u"community"_q, u"group"_q, u"collapse"_q });
	builder.addDividerText(tr::lng_nagram_disable_community_grouping_note());
	AddToggle(builder, Chats::kRecentChats,
		tr::lng_nagram_recent_chats_option(),
		u"regram/nagram/chats/recent-chats"_q,
		{ u"recent"_q, u"history"_q, u"main menu"_q });
	builder.addButton({
		.id = u"regram/nagram/chats/recent-chats-limit"_q,
		.title = tr::lng_nagram_recent_chats_limit(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Chats::kRecentChatsLimit)
			| rpl::map([](int value) { return QString::number(value); }),
		.onClick = [=] { controller->show(Box(RecentChatsLimitBox)); },
		.keywords = { u"recent"_q, u"limit"_q, u"count"_q },
	});
	builder.addDividerText(tr::lng_nagram_recent_chats_note());
	AddToggle(builder, Chats::kRecentInShare,
		tr::lng_nagram_recent_in_share(),
		u"regram/nagram/chats/recent-in-share"_q,
		{ u"recent"_q, u"forward"_q, u"share"_q });
	AddToggle(builder, Chats::kChatTools,
		tr::lng_nagram_chat_tools(),
		u"regram/nagram/chats/chat-tools"_q,
		{ u"toolbar"_q, u"top bar"_q, u"media"_q, u"pinned"_q });
	AddToggle(builder, Chats::kSaveReadingPosition,
		tr::lng_nagram_save_reading_position(),
		u"regram/nagram/chats/reading-position"_q,
		{ u"scroll"_q, u"position"_q, u"reading"_q });
	builder.addDividerText(tr::lng_nagram_save_reading_position_note());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/chats/local-pins"_q,
		.title = tr::lng_nagram_local_pins_group(),
		.keywords = { u"pin"_q, u"local"_q },
	});
	AddToggle(builder, Chats::kUnlimitedPinnedChats,
		tr::lng_nagram_unlimited_pinned_chats(),
		u"regram/nagram/chats/unlimited-pinned-chats"_q,
		{ u"pin"_q, u"unlimited"_q, u"limit"_q });
	builder.addDividerText(tr::lng_nagram_unlimited_pinned_chats_about());
	builder.addButton({
		.id = u"regram/nagram/chats/local-pinned-chats"_q,
		.title = tr::lng_nagram_local_pinned_chats(),
		.st = &st::settingsButtonNoIcon,
		.label = Chats::LocalPinsCountValue(session)
			| rpl::map([](int count) { return QString::number(count); }),
		.onClick = [=] {
			controller->show(Box(Chats::LocalPinsBox, session));
		},
		.keywords = { u"pin"_q, u"local"_q, u"unpin"_q },
		.shown = Chats::LocalPinsCountValue(session)
			| rpl::map([](int count) { return count > 0; }),
	});
	::Settings::BuildExtraChats(builder);
});

const SectionBuildMethod ChatsSection::kBuild = kMeta.build;

} // namespace

Settings::Type ChatsId() {
	return ChatsSection::Id();
}

} // namespace Nagram
