#include "regram/nagram/settings/compose.h"

#include "regram/nagram/compose/options.h"
#include "regram/nagram/compose/text.h"
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
#include "styles/style_settings.h"
#include "styles/style_layers.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class ComposeSection final : public Section<ComposeSection> {
public:
	ComposeSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_compose();
	}

	static const SectionBuildMethod kBuild;
};

void AddToggle(
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
}

QString PlaceholderLabel(int value) {
	switch (value) {
	case 1: return tr::lng_nagram_placeholder_chat(tr::now);
	case 2: return tr::lng_nagram_placeholder_sender(tr::now);
	default: return tr::lng_nagram_preview_follow(tr::now);
	}
}

void FormatItemsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_format_menu_items());
	const auto hidden = ForDevice().Get(Compose::kHiddenFormatItems);
	auto checks = std::vector<not_null<Ui::Checkbox*>>();
	for (auto i = 0; i != Compose::kFormatMenuItemCount; ++i) {
		checks.push_back(box->addRow(object_ptr<Ui::Checkbox>(
			box,
			Compose::FormatMenuItemTitle(i),
			!(hidden & (1 << i)))));
	}
	box->addButton(tr::lng_settings_save(), [=] {
		auto value = 0;
		for (auto i = 0; i != int(checks.size()); ++i) {
			if (!checks[i]->checked()) {
				value |= (1 << i);
			}
		}
		Expects(ForDevice().Set(Compose::kHiddenFormatItems, value));
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

QString InputChineseLabel(int value) {
	switch (value) {
	case 1: return tr::lng_nagram_reading_simplified(tr::now);
	case 2: return tr::lng_nagram_reading_traditional(tr::now);
	default: return tr::lng_nagram_reading_off(tr::now);
	}
}

void InputChineseBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_input_chinese());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Compose::kInputChinese));
	for (auto value = 0; value != 3; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, InputChineseLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Compose::kInputChinese, value));
		box->closeBox();
	});
}

void PlaceholderBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_input_placeholder());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Compose::kInputPlaceholderMode));
	for (auto value = 0; value != 3; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, PlaceholderLabel(value), st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Compose::kInputPlaceholderMode, value));
		box->closeBox();
	});
}

void CodeLanguageBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_default_code_language());
	const auto current = ForDevice().Get(Compose::kDefaultCodeLanguage);
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField,
		tr::lng_nagram_code_language_hint(), current));
	field->setMaxLength(32);
	field->setInputMethodHints(Qt::ImhLatinOnly
		| Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto save = [=] {
		const auto value = field->getLastText().trimmed();
		if (!Compose::kDefaultCodeLanguage.validate(value)) {
			field->showError();
			return;
		}
		Expects(ForDevice().Set(Compose::kDefaultCodeLanguage, value));
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { save(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void QuickRepliesBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_quick_replies());
	auto current = Compose::QuickReplies();
	if (current.size() != 2) {
		current = { QString(), QString() };
	}
	const auto first = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField,
		tr::lng_nagram_quick_reply_one(), current[0]));
	const auto second = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField,
		tr::lng_nagram_quick_reply_two(), current[1]));
	box->setFocusCallback([=] { first->setFocusFast(); });
	box->addButton(tr::lng_settings_save(), [=] {
		Expects(Compose::SetQuickReplies({
			first->getLastText(), second->getLastText() }));
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = ComposeSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_compose,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle({
		.id = u"regram/nagram/compose/buttons"_q,
		.title = tr::lng_nagram_compose_buttons(),
		.keywords = { u"buttons"_q, u"compose"_q },
	});
	AddToggle(builder, Compose::kHideAttachButton,
		tr::lng_nagram_hide_attach_button(),
		u"regram/nagram/compose/hide-attach"_q, { u"attach"_q });
	AddToggle(builder, Compose::kHideEmojiButton,
		tr::lng_nagram_hide_emoji_button(),
		u"regram/nagram/compose/hide-emoji"_q, { u"emoji"_q });
	AddToggle(builder, Compose::kHideRecordingButton,
		tr::lng_nagram_hide_recording_button(),
		u"regram/nagram/compose/hide-recording"_q, { u"recording"_q });
	AddToggle(builder, Compose::kHideBotCommandButton,
		tr::lng_nagram_hide_bot_command_button(),
		u"regram/nagram/compose/hide-bot-command"_q, { u"bot"_q, u"command"_q });
	AddToggle(builder, Compose::kHideBotMenu,
		tr::lng_nagram_hide_bot_menu(),
		u"regram/nagram/compose/hide-bot-menu"_q, { u"bot"_q, u"menu"_q });
	AddToggle(builder, Compose::kHideAutoDeleteButton,
		tr::lng_nagram_hide_auto_delete_button(),
		u"regram/nagram/compose/hide-auto-delete"_q, { u"delete"_q, u"timer"_q });
	AddToggle(builder, Compose::kHideGiftButton,
		tr::lng_nagram_hide_gift_button(),
		u"regram/nagram/compose/hide-gift"_q, { u"gift"_q });
	AddToggle(builder, Compose::kHideAiButton,
		tr::lng_nagram_hide_ai_button(),
		u"regram/nagram/compose/hide-ai"_q, { u"AI"_q });
	AddToggle(builder, Compose::kHideSendAsButton,
		tr::lng_nagram_hide_send_as_button(),
		u"regram/nagram/compose/hide-send-as"_q, { u"send as"_q });
	AddToggle(builder, Compose::kHideStarsReactionButton,
		tr::lng_nagram_hide_stars_reaction_button(),
		u"regram/nagram/compose/hide-stars-reaction"_q, { u"Stars"_q });
	AddToggle(builder, Compose::kHideChannelMuteButton,
		tr::lng_nagram_hide_channel_mute_button(),
		u"regram/nagram/compose/hide-channel-mute"_q, { u"channel"_q, u"mute"_q });
	AddToggle(builder, Compose::kChannelDiscussButton,
		tr::lng_nagram_channel_discuss_button(),
		u"regram/nagram/compose/channel-discuss"_q,
		{ u"channel"_q, u"discussion"_q, u"comments"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/compose/input-behavior"_q,
		.title = tr::lng_nagram_input_behavior(),
		.keywords = { u"input"_q, u"behavior"_q },
	});
	AddToggle(builder, Compose::kDisableEmojiHover,
		tr::lng_nagram_disable_emoji_hover(),
		u"regram/nagram/compose/disable-emoji-hover"_q,
		{ u"emoji"_q, u"hover"_q });
	AddToggle(builder, Compose::kDisableAttachHover,
		tr::lng_nagram_disable_attach_hover(),
		u"regram/nagram/compose/disable-attach-hover"_q,
		{ u"attachment"_q, u"hover"_q });
	AddToggle(builder, Compose::kBotCommandsToDraft,
		tr::lng_nagram_bot_commands_to_draft(),
		u"regram/nagram/compose/bot-commands-to-draft"_q,
		{ u"bot"_q, u"command"_q, u"draft"_q });
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/compose/input-placeholder"_q,
		.title = tr::lng_nagram_input_placeholder(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Compose::kInputPlaceholderMode)
			| rpl::map(PlaceholderLabel),
		.onClick = [=] { controller->show(Box(PlaceholderBox)); },
		.keywords = { u"placeholder"_q, u"hint"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/compose/text-format"_q,
		.title = tr::lng_nagram_text_format(),
		.keywords = { u"text"_q, u"format"_q },
	});
	AddToggle(builder, Compose::kFormatToolbar,
		tr::lng_nagram_format_toolbar(),
		u"regram/nagram/compose/format-toolbar"_q,
		{ u"format"_q, u"toolbar"_q, u"bold"_q });
	builder.addButton({
		.id = u"regram/nagram/compose/format-menu-items"_q,
		.title = tr::lng_nagram_format_menu_items(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Compose::kHiddenFormatItems)
			| rpl::map([](int hidden) {
				auto shown = Compose::kFormatMenuItemCount;
				for (auto i = 0; i != Compose::kFormatMenuItemCount; ++i) {
					shown -= (hidden >> i) & 1;
				}
				return QString::number(shown)
					+ u'/'
					+ QString::number(Compose::kFormatMenuItemCount);
			}),
		.onClick = [=] { controller->show(Box(FormatItemsBox)); },
		.keywords = { u"formatting"_q, u"menu"_q },
	});
	if (Messages::ChineseConversionAvailable()) {
		builder.addButton({
			.id = u"regram/nagram/compose/input-chinese"_q,
			.title = tr::lng_nagram_input_chinese(),
			.st = &st::settingsButtonNoIcon,
			.label = ForDevice().Value(Compose::kInputChinese)
				| rpl::map(InputChineseLabel),
			.onClick = [=] { controller->show(Box(InputChineseBox)); },
			.keywords = { u"traditional"_q, u"simplified"_q, u"Chinese"_q },
		});
	}
	AddToggle(builder, Compose::kDisableAutoMarkdown,
		tr::lng_nagram_disable_auto_markdown(),
		u"regram/nagram/compose/disable-auto-markdown"_q,
		{ u"Markdown"_q });
	AddToggle(builder, Compose::kDisableLinkPreview,
		tr::lng_nagram_disable_link_preview(),
		u"regram/nagram/compose/disable-link-preview"_q,
		{ u"link"_q, u"preview"_q });
	AddToggle(builder, Compose::kSpaceOnSend,
		tr::lng_nagram_space_on_send(),
		u"regram/nagram/compose/space-on-send"_q,
		{ u"spacing"_q, u"send"_q });
	AddToggle(builder, Compose::kSpaceOnEdit,
		tr::lng_nagram_space_on_edit(),
		u"regram/nagram/compose/space-on-edit"_q,
		{ u"spacing"_q, u"edit"_q });
	builder.addButton({
		.id = u"regram/nagram/compose/default-code-language"_q,
		.title = tr::lng_nagram_default_code_language(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Compose::kDefaultCodeLanguage)
			| rpl::map([](const QString &value) {
				return value.isEmpty()
					? tr::lng_nagram_preview_follow(tr::now)
					: value;
			}),
		.onClick = [=] { controller->show(Box(CodeLanguageBox)); },
		.keywords = { u"code"_q, u"language"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/compose/quick-replies"_q,
		.title = tr::lng_nagram_quick_replies(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Box(QuickRepliesBox)); },
		.keywords = { u"quick"_q, u"reply"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/compose/send-confirmation"_q,
		.title = tr::lng_nagram_send_confirmation(),
		.keywords = { u"send"_q, u"confirm"_q },
	});
	AddToggle(builder, Compose::kConfirmSticker,
		tr::lng_nagram_confirm_sticker(),
		u"regram/nagram/compose/confirm-sticker"_q,
		{ u"sticker"_q, u"confirm"_q });
	AddToggle(builder, Compose::kConfirmGif,
		tr::lng_nagram_confirm_gif(),
		u"regram/nagram/compose/confirm-gif"_q,
		{ u"GIF"_q, u"confirm"_q });
	AddToggle(builder, Compose::kPreviewVoice,
		tr::lng_nagram_preview_voice(),
		u"regram/nagram/compose/preview-voice"_q,
		{ u"voice"_q, u"listen"_q });
	AddToggle(builder, Compose::kPreviewRoundVideo,
		tr::lng_nagram_preview_round_video(),
		u"regram/nagram/compose/preview-round-video"_q,
		{ u"video"_q, u"preview"_q });
	AddToggle(builder, Compose::kConfirmPrivateCall,
		tr::lng_nagram_confirm_private_call(),
		u"regram/nagram/compose/confirm-private-call"_q,
		{ u"call"_q, u"confirm"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/compose/forwarding"_q,
		.title = tr::lng_nagram_forwarding(),
		.keywords = { u"forward"_q },
	});
	AddToggle(builder, Compose::kForwardBeforeComment,
		tr::lng_nagram_forward_before_comment(),
		u"regram/nagram/compose/forward-before-comment"_q,
		{ u"forward"_q, u"comment"_q, u"order"_q });
});

const SectionBuildMethod ComposeSection::kBuild = kMeta.build;

} // namespace

Settings::Type ComposeId() {
	return ComposeSection::Id();
}

} // namespace Nagram
