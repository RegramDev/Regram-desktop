#include "regram/nagram/settings/interface.h"
#include "regram/nagram/notifications/settings.h"

#include "regram/nagram/interface/app_icon.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/ayu/ui/ayu_logo.h"
#include "base/platform/base_platform_info.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/interface/main_menu.h"
#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/restart.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_nagram_interface.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class InterfaceSection final : public Section<InterfaceSection> {
public:
	InterfaceSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_interface();
	}

	static const SectionBuildMethod kBuild;
};

constexpr auto kMinRoundness = 10;
constexpr auto kMaxRoundness = 100;

QString RoundnessLabel(int value) {
	return (value
		? QString::number(value) + u"%"_q
		: tr::lng_nagram_preview_follow(tr::now))
		+ u" · "_q + tr::lng_nagram_restart_required(tr::now);
}

void RoundnessBox(
		not_null<Ui::GenericBox*> box,
		const Option<int> &option,
		QString title,
		not_null<Window::SessionController*> controller) {
	box->setTitle(title);
	const auto current = ForDevice().Get(option);
	const auto follow = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_nagram_preview_follow(tr::now),
		!current,
		st::defaultBoxCheckbox));
	auto row = MakeSliderWithLabel(
		box,
		st::settingsScale,
		st::settingsScaleLabel,
		st::normalFont->spacew * 2,
		st::settingsScaleLabel.style.font->width(u"100%"_q),
		true);
	const auto slider = row.slider;
	const auto label = row.label;
	box->addRow(std::move(row.widget), st::settingsBigScalePadding);
	slider->setAccessibleName(title);

	const auto value = box->lifetime().make_state<int>(
		current ? current : kMaxRoundness);
	const auto show = [=](int percent) {
		*value = percent;
		label->setText(QString::number(percent) + u"%"_q);
	};
	slider->setPseudoDiscrete(
		kMaxRoundness - kMinRoundness + 1,
		[](int index) { return kMinRoundness + index; },
		*value,
		show);
	show(*value);
	follow->checkedValue(
	) | rpl::on_next([=](bool checked) {
		slider->setDisabled(checked);
	}, slider->lifetime());

	box->addButton(tr::lng_settings_save(), [=] {
		const auto chosen = follow->checked() ? 0 : *value;
		if (chosen != current) {
			Expects(ForDevice().Set(option, chosen));
			ShowRestartPrompt(controller);
		}
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void AddRoundness(
		SectionBuilder &builder,
		const Option<int> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(option) | rpl::map(RoundnessLabel),
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				RoundnessBox(box, option, (option.key == Interface::kBubbleRoundness.key)
					? tr::lng_nagram_bubble_roundness(tr::now)
					: tr::lng_nagram_avatar_roundness(tr::now), controller);
			}));
		},
		.keywords = std::move(keywords),
	});
}

void TextWidthBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_text_message_width());
	const auto current = ForDevice().Get(Interface::kTextMessageWidth);
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_text_width_hint(),
		current ? QString::number(current) : QString()));
	field->setInputMethodHints(Qt::ImhDigitsOnly);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		const auto text = field->getLastText().trimmed();
		auto valid = false;
		const auto value = text.isEmpty() ? 0 : text.toInt(&valid);
		if ((!text.isEmpty() && !valid)
			|| (value != 0 && (value < 50 || value > 400))) {
			field->showError();
			return;
		}
		if (value != current) {
			Expects(ForDevice().Set(Interface::kTextMessageWidth, value));
		}
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

QString DelayLabel(int milliseconds) {
	return milliseconds
		? QString::number(milliseconds / 1000.0)
			+ tr::lng_nagram_seconds_suffix(tr::now)
		: tr::lng_nagram_preview_follow(tr::now);
}

void DelayBox(
		not_null<Ui::GenericBox*> box,
		const Option<int> &option,
		QString title) {
	box->setTitle(std::move(title));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(option));
	for (const auto &value : { 0, 500, 1000, 2000, 5000, 10000, 30000, 60000 }) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, DelayLabel(value), st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(option, value));
		box->closeBox();
	});
}

void AddDelay(
		SectionBuilder &builder,
		const Option<int> &option,
		rpl::producer<QString> title,
		QString id) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(option) | rpl::map(DelayLabel),
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				DelayBox(box, option, option.key == Interface::kNotificationDelay.key
					? tr::lng_nagram_notification_delay(tr::now)
					: tr::lng_nagram_other_device_notification_delay(tr::now));
			}));
		},
		.keywords = { u"notification"_q, u"delay"_q },
	});
}

void AppIconBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_app_icon());
	const auto choices = Interface::AppIconChoices();
	const auto current = ForDevice().Get(Interface::kAppIcon);
	const auto shown = box->lifetime().make_state<QString>(current);
	const auto size = st::nagramAppIconPreview;
	const auto preview = box->addRow(
		object_ptr<Ui::RpWidget>(box),
		st::nagramAppIconPreviewMargin);
	preview->resize(st::boxWidth, size);
	preview->paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(preview);
		const auto ratio = style::DevicePixelRatio();
		auto image = Interface::AppIconPreview(*shown).scaled(
			QSize(size, size) * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		image.setDevicePixelRatio(ratio);
		p.drawImage((preview->width() - size) / 2, 0, image);
	}, preview->lifetime());
	auto selected = 0;
	for (auto i = 0; i != int(choices.size()); ++i) {
		if (choices[i].id == current) {
			selected = i;
		}
	}
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	for (auto i = 0; i != int(choices.size()); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, i, choices[i].title, st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		*shown = choices[value].id;
		AyuSettings::getInstance().setAppIcon(AyuAssets::DEFAULT_ICON);
		Expects(ForDevice().Set(Interface::kAppIcon, *shown));
		preview->update();
	});
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_app_icon_about(),
			st::boxDividerLabel),
		st::settingsSendTypePadding);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

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
			if (option.flags & Interface::kRestart) {
				ShowRestartPrompt(controller);
			}
		}, button->lifetime());
	}
}

const auto kMeta = BuildHelper({
	.id = InterfaceSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_interface,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle({
		.id = u"regram/nagram/interface/roundness"_q,
		.title = tr::lng_nagram_roundness_and_shapes(),
		.keywords = { u"corners"_q, u"shapes"_q },
	});
	AddRoundness(builder, Interface::kBubbleRoundness,
		tr::lng_nagram_bubble_roundness(),
		u"regram/nagram/interface/bubble-roundness"_q,
		{ u"bubble"_q, u"roundness"_q });
	AddRoundness(builder, Interface::kAvatarRoundness,
		tr::lng_nagram_avatar_roundness(),
		u"regram/nagram/interface/avatar-roundness"_q,
		{ u"avatar"_q, u"roundness"_q });
	const auto controller = builder.controller();
	const auto button = builder.addButton({
		.id = u"regram/nagram/interface/uniform-avatars"_q,
		.title = tr::lng_nagram_uniform_avatar_shapes(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(tr::lng_nagram_restart_required(tr::now)),
		.toggled = ForDevice().Value(Interface::kUniformAvatarShapes),
		.keywords = { u"forum"_q, u"channel"_q, u"avatar"_q },
		.shown = ForDevice().Value(Interface::kAvatarRoundness)
			| rpl::map([](int value) { return value != 0; }),
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([=](bool value) {
			Expects(ForDevice().Set(Interface::kUniformAvatarShapes, value));
			ShowRestartPrompt(controller);
		}, button->lifetime());
	}
	builder.addSubsectionTitle({
		.id = u"regram/nagram/interface/message-style"_q,
		.title = tr::lng_nagram_message_style(),
		.keywords = { u"messages"_q, u"style"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/interface/text-width"_q,
		.title = tr::lng_nagram_text_message_width(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Interface::kTextMessageWidth)
			| rpl::map([](int value) {
				return value ? QString::number(value) + u"%"_q
					: tr::lng_nagram_preview_follow(tr::now);
			}),
		.onClick = [=] { controller->show(Box(TextWidthBox)); },
		.keywords = { u"text"_q, u"width"_q },
	});
	AddToggle(builder, Interface::kWideChannelPosts,
		tr::lng_nagram_wide_channel_posts(),
		u"regram/nagram/interface/wide-channel-posts"_q,
		{ u"channel"_q, u"width"_q });
	AddToggle(builder, Interface::kHideBubbleTail,
		tr::lng_nagram_hide_bubble_tail(),
		u"regram/nagram/interface/hide-bubble-tail"_q,
		{ u"bubble"_q, u"tail"_q });
	AddToggle(builder, Interface::kThemeReplyColors,
		tr::lng_nagram_theme_reply_colors(),
		u"regram/nagram/interface/theme-reply-colors"_q,
		{ u"reply"_q, u"quote"_q, u"color"_q });
	AddToggle(builder, Interface::kHideReplyThumbnail,
		tr::lng_nagram_hide_reply_thumbnail(),
		u"regram/nagram/interface/hide-reply-thumbnail"_q,
		{ u"reply"_q, u"thumbnail"_q });
	AddToggle(builder, Interface::kIgnoreChatTheme,
		tr::lng_nagram_ignore_chat_theme(),
		u"regram/nagram/interface/ignore-chat-theme"_q,
		{ u"chat"_q, u"theme"_q, u"wallpaper"_q });
	AddToggle(builder, Interface::kIgnorePrivateChatTheme,
		tr::lng_nagram_ignore_private_chat_theme(),
		u"regram/nagram/interface/ignore-private-theme"_q,
		{ u"private"_q, u"theme"_q, u"wallpaper"_q });
	AddToggle(builder, Interface::kIgnoreChannelChatTheme,
		tr::lng_nagram_ignore_channel_chat_theme(),
		u"regram/nagram/interface/ignore-channel-theme"_q,
		{ u"channel"_q, u"theme"_q, u"wallpaper"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/interface/main-menu-heading"_q,
		.title = tr::lng_nagram_main_menu(),
		.keywords = { u"menu"_q, u"title"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/interface/main-menu"_q,
		.title = tr::lng_nagram_main_menu(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Box(Interface::MainMenuBox)); },
		.keywords = { u"menu"_q, u"order"_q, u"visibility"_q },
	});
	AddToggle(builder, Interface::kAlwaysSeasonal,
		tr::lng_nagram_always_seasonal(),
		u"regram/nagram/interface/always-seasonal"_q,
		{ u"holiday"_q, u"snow"_q, u"decoration"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/interface/window-notification"_q,
		.title = tr::lng_nagram_window_notification(),
		.keywords = { u"window"_q, u"notification"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/interface/app-icon"_q,
		.title = tr::lng_nagram_app_icon(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Interface::kAppIcon)
			| rpl::map(Interface::AppIconTitle),
		.onClick = [=] { controller->show(Box(AppIconBox)); },
		.keywords = { u"icon"_q, u"dock"_q, u"taskbar"_q },
	});
	AddToggle(builder, Interface::kHideAppIconBadge,
		tr::lng_nagram_hide_app_icon_badge(),
		u"regram/nagram/interface/hide-app-icon-badge"_q,
		{ u"dock"_q, u"icon"_q, u"badge"_q });
	AddToggle(builder, Interface::kAccountNameInTitle,
		tr::lng_nagram_account_name_in_title(),
		u"regram/nagram/interface/account-name-title"_q,
		{ u"window"_q, u"title"_q, u"account"_q });
	AddDelay(builder, Interface::kNotificationDelay,
		tr::lng_nagram_notification_delay(),
		u"regram/nagram/interface/notification-delay"_q);
	AddDelay(builder, Interface::kOtherDeviceNotificationDelay,
		tr::lng_nagram_other_device_notification_delay(),
		u"regram/nagram/interface/other-device-notification-delay"_q);
	builder.addButton({
		.id = u"regram/nagram/interface/quiet-hours"_q,
		.title = tr::lng_nagram_quiet_hours(),
		.st = &st::settingsButtonNoIcon,
		.label = Notifications::QuietHoursLabel(),
		.onClick = [=] {
			controller->show(Box(Notifications::QuietHoursBox));
		},
		.keywords = { u"notification"_q, u"quiet"_q, u"mute"_q, u"night"_q },
	});
	builder.addSubsectionTitle({
		.id = u"regram/nagram/interface/text"_q,
		.title = tr::lng_nagram_ui_text(),
		.keywords = { u"text"_q, u"punctuation"_q },
	});
	AddToggle(builder, Interface::kHalfwidthUiPunctuation,
		tr::lng_nagram_halfwidth_ui_punctuation(),
		u"regram/nagram/interface/halfwidth-ui-punctuation"_q,
		{ u"text"_q, u"punctuation"_q });
});

const SectionBuildMethod InterfaceSection::kBuild = kMeta.build;

} // namespace

Settings::Type InterfaceId() {
	return InterfaceSection::Id();
}

} // namespace Nagram
