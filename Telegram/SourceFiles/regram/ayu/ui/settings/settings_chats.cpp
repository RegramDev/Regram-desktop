// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/ui/settings/settings_chats.h"

#include "lang_auto.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/ayu/ui/boxes/edit_mark_box.h"
#include "regram/ayu/ui/components/message_preview.h"
#include "regram/ayu/ui/settings/ayu_builder.h"
#include "regram/ayu/ui/settings/settings_ayu_utils.h"
#include "regram/ayu/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <memory>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

struct PreviewState {
	MessagePreview *widget = nullptr;
};

void BuildStickersAndEmoji(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::lng_settings_stickers_emoji());

	ayu.addSettingToggle({
		.id = u"regram/ayu/showOnlyAddedEmojisAndStickers"_q,
		.title = tr::ayu_ShowOnlyAddedEmojisAndStickers(),
		.getter = &AyuSettings::showOnlyAddedEmojisAndStickers,
		.setter = &AyuSettings::setShowOnlyAddedEmojisAndStickers,
	});


	ayu.addSectionDivider();
}

void BuildGroupsAndChannels(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::lng_premium_double_limits_subtitle_channels());

	ayu.addChooseButton({
		.id = u"regram/ayu/channelBottomButton"_q,
		.altIds = { u"regram/ayu/bottomButton"_q },
		.title = tr::ayu_ChannelBottomButton(),
		.boxTitle = tr::ayu_ChannelBottomButton(),
		.initialSelection = static_cast<int>(settings->channelBottomButton()),
		.options = {
			tr::ayu_ChannelBottomButtonHide(tr::now),
			tr::ayu_ChannelBottomButtonMute(tr::now),
			tr::ayu_ChannelBottomButtonDiscuss(tr::now),
		},
		.setter = [](int index) {
			AyuSettings::getInstance().setChannelBottomButton(
				static_cast<ChannelBottomButton>(index));
		},
	});

	ayu.addSettingToggle({
		.id = u"regram/ayu/quickAdminShortcuts"_q,
		.title = tr::ayu_QuickAdminShortcuts(),
		.getter = &AyuSettings::quickAdminShortcuts,
		.setter = &AyuSettings::setQuickAdminShortcuts,
	});

	ayu.addSettingToggle({
		.id = u"regram/ayu/showMessageShot"_q,
		.title = tr::ayu_SettingsShowMessageShot(),
		.getter = &AyuSettings::showMessageShot,
		.setter = &AyuSettings::setShowMessageShot,
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsShowMessageShotDescription());
	builder.addSkip();
}

void BuildMarks(
		SectionBuilder &builder,
		AyuSectionBuilder &ayu,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::lng_settings_messages());

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<MessagePreview>(ctx.container, controller);
		previewState->widget = preview.data();
		return {
			.widget = std::move(preview),
			.margin = style::margins(
				0,
				st::defaultVerticalListSkip,
				0,
				st::settingsPrivacySkipTop),
		};
	});

	ayu.addSettingToggle({
		.id = u"regram/ayu/replaceBottomInfoWithIcons"_q,
		.altIds = { u"regram/ayu/replaceEditedWithIcon"_q },
		.title = tr::ayu_ReplaceMarksWithIcons(),
		.getter = &AyuSettings::replaceBottomInfoWithIcons,
		.setter = &AyuSettings::setReplaceBottomInfoWithIcons,
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"regram/ayu/deletedMark"_q,
			.title = tr::ayu_DeletedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AyuSettings::getInstance().deletedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ayu_DeletedMarkText(),
					settings->deletedMark(),
					QString("🧹"),
					[=](const QString &value) {
						AyuSettings::getInstance().setDeletedMark(value);
					});
				Ui::show(std::move(box));
			},
		});

		builder.addButton({
			.id = u"regram/ayu/editedMark"_q,
			.title = tr::ayu_EditedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AyuSettings::getInstance().editedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ayu_EditedMarkText(),
					settings->editedMark(),
					tr::lng_edited(tr::now),
					[=](const QString &value) {
						AyuSettings::getInstance().setEditedMark(value);
					});
				Ui::show(std::move(box));
			},
		});
	}, AyuSettings::getInstance().replaceBottomInfoWithIconsValue()
		| rpl::map([](bool v) { return !v; }));

	const auto semiTransparent = ayu.addSettingToggle({
		.id = u"regram/ayu/semiTransparentDeletedMessages"_q,
		.altIds = { u"regram/ayu/translucentDeletedMessages"_q },
		.title = tr::ayu_SemiTransparentDeletedMessages(),
		.getter = &AyuSettings::semiTransparentDeletedMessages,
		.setter = &AyuSettings::setSemiTransparentDeletedMessages,
	});
	if (semiTransparent) {
		ayu.addBetaBadge(semiTransparent);
	}

	ayu.addSectionDivider();
}

void BuildWideMessagesMultiplier(
		SectionBuilder &builder,
		AyuSectionBuilder &ayu,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AyuSettings::getInstance();

	constexpr auto kMinSize = 1.00;
	constexpr auto kStep = 0.05;

	const auto valueToIndex = [=](double value) {
		return static_cast<int>(std::round((value - kMinSize) / kStep));
	};

	const auto controller = builder.controller();
	ayu.addSlider({
		.id = u"regram/ayu/messageBubbleRadius"_q,
		.title = tr::ayu_MessageBubbleRadius(),
		.steps = 17,
		.current = settings->messageBubbleRadius(),
		.indexToValue = [](int index) { return index; },
		.onChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
		},
		.onFinalChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
			AyuSettings::getInstance().setMessageBubbleRadius(index);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [](int index) {
			return QString::number(index);
		},
	});

	ayu.addSectionDivider();

	ayu.addSlider({
		.id = u"regram/ayu/wideMultiplier"_q,
		.title = tr::ayu_SettingsWideMultiplier(),
		.steps = 61, // (4.00 - 1.00) / 0.05 + 1
		.current = valueToIndex(settings->wideMultiplier()),
		.indexToValue = [](int index) { return index; },
		.onChanged = nullptr,
		.onFinalChanged = [=](int index) {
			AyuSettings::getInstance().setWideMultiplier(
				kMinSize + index * kStep);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [=](int index) {
			return QString::number(kMinSize + index * kStep, 'f', 2);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsWideMultiplierDescription());
	builder.addSkip();
}

void BuildContextMenuElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_ContextMenuElementsHeader());

	const auto options = std::vector{
		tr::ayu_SettingsContextMenuItemHidden(tr::now),
		tr::ayu_SettingsContextMenuItemShown(tr::now),
		tr::ayu_SettingsContextMenuItemExtended(tr::now),
	};

	ayu.addChooseButton({
		.id = u"regram/ayu/showReactionsPanelInContextMenu"_q,
		.title = tr::ayu_SettingsContextMenuReactionsPanel(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showReactionsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowReactionsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconReactions },
	});
	ayu.addChooseButton({
		.id = u"regram/ayu/showViewsPanelInContextMenu"_q,
		.title = tr::ayu_SettingsContextMenuViewsPanel(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showViewsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowViewsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconShowInChat },
	});
	ayu.addChooseButton({
		.id = u"regram/ayu/showHideMessageInContextMenu"_q,
		.title = tr::ayu_ContextHideMessage(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showHideMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowHideMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconClear },
	});
	ayu.addChooseButton({
		.id = u"regram/ayu/showUserMessagesInContextMenu"_q,
		.title = tr::ayu_UserMessagesMenuText(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showUserMessagesInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowUserMessagesInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconTTL },
	});
	ayu.addChooseButton({
		.id = u"regram/ayu/showMessageDetailsInContextMenu"_q,
		.title = tr::ayu_MessageDetailsPC(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showMessageDetailsInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowMessageDetailsInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconInfo },
	});
	ayu.addChooseButton({
		.id = u"regram/ayu/showRepeatMessageInContextMenu"_q,
		.title = tr::ayu_RepeatMessage(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showRepeatMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowRepeatMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::ayuRepeatMenuIcon },
	});
	if (settings->filtersEnabled()) {
		ayu.addChooseButton({
			.id = u"regram/ayu/showAddFilterInContextMenu"_q,
			.title = tr::ayu_RegexFilterQuickAdd(),
			.boxTitle = tr::ayu_SettingsContextMenuTitle(),
			.initialSelection = static_cast<int>(settings->showAddFilterInContextMenu()),
			.options = options,
			.setter = [](int i) { AyuSettings::getInstance().setShowAddFilterInContextMenu(static_cast<ContextMenuVisibility>(i)); },
			.icon = { &st::menuIconAddToFolder },
		});
	}

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsContextMenuDescription());
	builder.addSkip();
}

void BuildMessageFieldPopups(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_MessageFieldPopupsHeader());

	ayu.addSettingToggle({
		.id = u"regram/ayu/showAttachPopup"_q,
		.title = tr::ayu_MessageFieldElementAttach(),
		.getter = &AyuSettings::showAttachPopup,
		.setter = &AyuSettings::setShowAttachPopup,
		.icon = { &st::messageFieldAttachIcon },
	});

}

} // namespace

void BuildExtraMessages(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	const auto previewState = std::make_shared<PreviewState>();
	BuildMarks(builder, ayu, previewState);
	BuildWideMessagesMultiplier(builder, ayu, previewState);
}

void BuildExtraChats(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildGroupsAndChannels(builder, ayu);
}

void BuildExtraMedia(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildStickersAndEmoji(builder, ayu);
}

void BuildExtraCompose(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildMessageFieldPopups(builder, ayu);
	ayu.addSettingToggle({
		.id = u"regram/ayu/voiceConfirmation"_q,
		.title = tr::ayu_VoiceConfirmation(),
		.getter = &AyuSettings::voiceConfirmation,
		.setter = &AyuSettings::setVoiceConfirmation,
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/roundConfirmation"_q,
		.title = tr::ayu_RoundConfirmation(),
		.getter = &AyuSettings::roundConfirmation,
		.setter = &AyuSettings::setRoundConfirmation,
	});
}

void BuildExtraMessageMenu(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildContextMenuElements(builder, ayu);
}

} // namespace Settings
