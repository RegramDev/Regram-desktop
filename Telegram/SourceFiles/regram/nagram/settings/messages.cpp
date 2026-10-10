#include "regram/nagram/settings/messages.h"
#include "regram/ayu/ui/settings/settings_chats.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/messages/reading.h"
#include "regram/nagram/settings/home.h"
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
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class MessagesSection final : public Section<MessagesSection> {
public:
	MessagesSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_messages();
	}

	static const SectionBuildMethod kBuild;
};

Ui::SettingsButton *AddToggle(
		SectionBuilder &builder,
		const Option<bool> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(option),
		.keywords = std::move(keywords),
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([option](bool value) {
			Expects(ForDevice().Set(option, value));
		}, button->lifetime());
	}
	return button;
}

void AddReactionChild(
		SectionBuilder &builder,
		const Option<bool> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	if (const auto button = AddToggle(builder, option,
			std::move(title), std::move(id), std::move(keywords))) {
		ForDevice().Value(Messages::kHideReactions
		) | rpl::on_next([button](bool hidden) {
			button->setDisabled(hidden);
			button->setEnabled(!hidden);
		}, button->lifetime());
	}
}

void EditMarkBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_edited_mark());
	const auto field = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			tr::lng_edited(),
			ForDevice().Get(Messages::kEditedMark)),
		st::boxRowPadding);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		if (ForDevice().Set(Messages::kEditedMark, field->getLastText())) {
			box->closeBox();
		}
	};
	field->submits() | rpl::on_next(submit, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

QString ReadingChineseLabel(int value) {
	switch (value) {
	case 1: return tr::lng_nagram_reading_simplified(tr::now);
	case 2: return tr::lng_nagram_reading_traditional(tr::now);
	default: return tr::lng_nagram_reading_off(tr::now);
	}
}

QString SenderOnlineLabel(int value) {
	switch (value) {
	case 1: return tr::lng_nagram_sender_online_now(tr::now);
	case 2: return tr::lng_nagram_sender_online_recently(tr::now);
	default: return tr::lng_nagram_reading_off(tr::now);
	}
}

void SenderOnlineBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_sender_online());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Messages::kSenderOnlineStatus));
	for (auto value = 0; value != 3; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, SenderOnlineLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Messages::kSenderOnlineStatus, value));
		box->closeBox();
	});
}

void ReadingChineseBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_reading_chinese());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Messages::kReadingChinese));
	for (auto value = 0; value != 3; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, ReadingChineseLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Messages::kReadingChinese, value));
		box->closeBox();
	});
}

const auto kMeta = BuildHelper({
	.id = MessagesSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_messages,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle({
		.id = u"regram/nagram/messages/time"_q,
		.title = tr::lng_nagram_time_and_info(),
		.keywords = { u"time"_q, u"info"_q },
	});
	AddToggle(builder, Messages::kSecondsInMessages,
		tr::lng_nagram_seconds_in_messages(),
		u"regram/nagram/messages/seconds"_q,
		{ u"seconds"_q, u"timestamp"_q });
	AddToggle(builder, Messages::kShowForwardedMessageDate,
		tr::lng_nagram_show_forwarded_message_date(),
		u"regram/nagram/messages/forwarded-date"_q,
		{ u"forwarded"_q, u"original time"_q });
	AddToggle(builder, Messages::kShowServiceTime,
		tr::lng_nagram_show_service_time(),
		u"regram/nagram/messages/service-time"_q,
		{ u"service"_q, u"time"_q });
	AddToggle(builder, Messages::kShowMessageId,
		tr::lng_nagram_show_message_id(),
		u"regram/nagram/messages/id"_q,
		{ u"message ID"_q, u"tooltip"_q });
	const auto onlineController = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/messages/sender-online"_q,
		.title = tr::lng_nagram_sender_online(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Messages::kSenderOnlineStatus)
			| rpl::map(SenderOnlineLabel),
		.onClick = [=] { onlineController->show(Box(SenderOnlineBox)); },
		.keywords = { u"online"_q, u"last seen"_q, u"avatar"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/messages/marks"_q,
		.title = tr::lng_nagram_marks_and_counts(),
		.keywords = { u"labels"_q, u"counts"_q },
	});
	AddToggle(builder, Messages::kExactMessageCounters,
		tr::lng_nagram_exact_message_counters(),
		u"regram/nagram/messages/exact-counters"_q,
		{ u"exact"_q, u"views"_q, u"replies"_q });
	AddToggle(builder, Messages::kShowForwardCount,
		tr::lng_nagram_show_forward_count(),
		u"regram/nagram/messages/forward-count"_q,
		{ u"forwards"_q, u"shares"_q, u"count"_q });
	AddToggle(builder, Messages::kHideMessageViews,
		tr::lng_nagram_hide_message_views(),
		u"regram/nagram/messages/hide-views"_q,
		{ u"hide"_q, u"views"_q });
	AddToggle(builder, Messages::kHideChannelSignature,
		tr::lng_nagram_hide_channel_signature(),
		u"regram/nagram/messages/hide-signature"_q,
		{ u"channel"_q, u"signature"_q });
	AddToggle(builder, Messages::kHideEditedBadge,
		tr::lng_nagram_hide_edited_badge(),
		u"regram/nagram/messages/hide-edited"_q,
		{ u"edited"_q, u"badge"_q });
	AddToggle(builder, Messages::kEditedIcon,
		tr::lng_nagram_edited_icon(),
		u"regram/nagram/messages/edited-icon"_q,
		{ u"edited"_q, u"icon"_q });
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/messages/edited-mark"_q,
		.title = tr::lng_nagram_edited_mark(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Messages::kEditedMark)
			| rpl::map([](const QString &text) {
				return text.isEmpty()
					? tr::lng_settings_notifications_display_default(tr::now)
					: text;
			}),
		.onClick = [=] { controller->show(Box(EditMarkBox)); },
		.keywords = { u"edited"_q, u"label"_q, u"text"_q },
		.shown = rpl::combine(
			ForDevice().Value(Messages::kHideEditedBadge),
			ForDevice().Value(Messages::kEditedIcon)
		) | rpl::map([](bool hidden, bool icon) { return !hidden && !icon; }),
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/messages/reactions"_q,
		.title = tr::lng_nagram_reactions(),
		.keywords = { u"reactions"_q },
	});
	AddToggle(builder, Messages::kHideReactions,
		tr::lng_nagram_hide_reactions(),
		u"regram/nagram/messages/hide-reactions"_q,
		{ u"hide"_q, u"reactions"_q });
	AddReactionChild(builder, Messages::kHidePrivateReactions,
		tr::lng_nagram_hide_private_reactions(),
		u"regram/nagram/messages/reactions-private"_q,
		{ u"private"_q, u"chat reactions"_q });
	AddReactionChild(builder, Messages::kHideGroupReactions,
		tr::lng_nagram_hide_group_reactions(),
		u"regram/nagram/messages/reactions-group"_q,
		{ u"group"_q, u"chat reactions"_q });
	AddReactionChild(builder, Messages::kHideChannelReactions,
		tr::lng_nagram_hide_channel_reactions(),
		u"regram/nagram/messages/reactions-channel"_q,
		{ u"channel"_q, u"chat reactions"_q });
	AddToggle(builder, Messages::kHideReactionMenu,
		tr::lng_nagram_hide_reaction_menu(),
		u"regram/nagram/messages/reaction-menu"_q,
		{ u"context menu"_q, u"reaction panel"_q });
	AddToggle(builder, Messages::kHideReactionMenuWhenSelecting,
		tr::lng_nagram_hide_reaction_menu_when_selecting(),
		u"regram/nagram/messages/reaction-selection"_q,
		{ u"selection"_q, u"reaction panel"_q });
	AddToggle(builder, Messages::kMoveWhoReactedToTop,
		tr::lng_nagram_move_who_reacted_to_top(),
		u"regram/nagram/messages/who-reacted-top"_q,
		{ u"seen by"_q, u"reactions"_q, u"top"_q, u"context menu"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/messages/effects"_q,
		.title = tr::lng_nagram_effects(),
		.keywords = { u"effects"_q, u"animation"_q },
	});
	AddToggle(builder, Messages::kDisablePremiumStickerEffects,
		tr::lng_nagram_disable_premium_sticker_effects(),
		u"regram/nagram/messages/effects-premium"_q,
		{ u"Premium"_q, u"sticker effects"_q });
	AddToggle(builder, Messages::kDisableEmojiInteractions,
		tr::lng_nagram_disable_emoji_interactions(),
		u"regram/nagram/messages/effects-emoji"_q,
		{ u"emoji"_q, u"interactions"_q });
	AddToggle(builder, Messages::kDisableMessageEffects,
		tr::lng_nagram_disable_message_effects(),
		u"regram/nagram/messages/effects-message"_q,
		{ u"message effects"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/messages/content"_q,
		.title = tr::lng_nagram_content_display(),
		.keywords = { u"content"_q, u"display"_q },
	});
	AddToggle(builder, Messages::kRevealSpoilers,
		tr::lng_nagram_reveal_spoilers(),
		u"regram/nagram/messages/reveal-spoilers"_q,
		{ u"reveal"_q, u"spoilers"_q });
	builder.addDividerText(tr::lng_nagram_reveal_spoilers_note());
	AddToggle(builder, Messages::kHideQuickShare,
		tr::lng_nagram_hide_quick_share(),
		u"regram/nagram/messages/hide-quick-share"_q,
		{ u"quick forward"_q, u"share"_q });
	AddToggle(builder, Messages::kHideRecommendedChannels,
		tr::lng_nagram_hide_recommended_channels(),
		u"regram/nagram/messages/hide-recommended"_q,
		{ u"recommended"_q, u"channels"_q });
	AddToggle(builder, Messages::kCollapseRecommendedChannels,
		tr::lng_nagram_collapse_recommended_channels(),
		u"regram/nagram/messages/collapse-recommended"_q,
		{ u"similar"_q, u"channels"_q, u"collapse"_q });
	AddToggle(builder, Messages::kSimplifiedReplies,
		tr::lng_nagram_simplified_replies(),
		u"regram/nagram/messages/simplified-replies"_q,
		{ u"reply"_q, u"quote"_q, u"simple"_q });
	AddToggle(builder, Messages::kDisableVideoUserpics,
		tr::lng_nagram_disable_video_userpics(),
		u"regram/nagram/messages/disable-video-userpics"_q,
		{ u"animated"_q, u"avatar"_q, u"video"_q });
	AddToggle(builder, Messages::kHidePremiumBadges,
		tr::lng_nagram_hide_premium_badges(),
		u"regram/nagram/messages/hide-premium"_q,
		{ u"Premium"_q, u"emoji status"_q });
	AddToggle(builder, Messages::kHideSavedTags,
		tr::lng_nagram_hide_saved_tags(),
		u"regram/nagram/messages/hide-tags"_q,
		{ u"saved messages"_q, u"tags"_q });
	AddToggle(builder, Messages::kHidePrivateChatActivities,
		tr::lng_nagram_hide_private_chat_activities(),
		u"regram/nagram/messages/hide-private-activity"_q,
		{ u"typing"_q, u"recording"_q, u"private chat"_q });
	builder.addDividerText(tr::lng_nagram_private_activities_note());
	AddToggle(builder, Messages::kReadingSpacing,
		tr::lng_nagram_reading_spacing(),
		u"regram/nagram/messages/reading-spacing"_q,
		{ u"reading"_q, u"spacing"_q });
	const auto chinese = builder.addButton({
		.id = u"regram/nagram/messages/reading-chinese"_q,
		.title = tr::lng_nagram_reading_chinese(),
		.st = &st::settingsButtonNoIcon,
		.label = Messages::ChineseConversionAvailable()
			? rpl::producer<QString>(ForDevice().Value(Messages::kReadingChinese)
				| rpl::map([](int value) { return ReadingChineseLabel(value); }))
			: rpl::producer<QString>(rpl::single(
				tr::lng_nagram_reading_unavailable(tr::now))),
		.onClick = [=] { controller->show(Box(ReadingChineseBox)); },
		.keywords = { u"Chinese"_q, u"simplified"_q, u"traditional"_q },
	});
	if (chinese && !Messages::ChineseConversionAvailable()) {
		chinese->setDisabled(true);
	}
	builder.addDividerText(tr::lng_nagram_reading_chinese_note());
	::Settings::BuildExtraMessages(builder);
});

const SectionBuildMethod MessagesSection::kBuild = kMeta.build;

} // namespace

Settings::Type MessagesId() {
	return MessagesSection::Id();
}

} // namespace Nagram
