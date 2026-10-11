// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/ui/settings/settings_appearance.h"

#include "lang_auto.h"
#include "regram/ayu/ayu_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "regram/ayu/ui/boxes/font_selector.h"
#include "regram/ayu/ui/components/avatar_corners_preview.h"
#include "regram/ayu/ui/components/icon_picker.h"
#include "regram/ayu/ui/settings/ayu_builder.h"
#include "regram/ayu/ui/settings/settings_ayu_utils.h"
#include "regram/ayu/ui/settings/settings_main.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

bool HasDrawerBots(not_null<Window::SessionController*> controller) {
	// todo: maybe iterate through all accounts
	const auto bots = &controller->session().attachWebView();
	for (const auto &bot : bots->attachBots()) {
		if (!bot.inMainMenu || !bot.media) {
			continue;
		}
		return true;
	}
	return false;
}

void BuildAvatarCorners(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	const auto mapRadius = [](int val)
	{
		if (val == 0) {
			return tr::ayu_AvatarCornersSquare(tr::now).toUpper();
		} else if (val == AyuUiSettings::kMaxAvatarCorners) {
			return tr::ayu_AvatarCornersCircle(tr::now).toUpper();
		}
		return QString::number(val);
	};

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto title = object_ptr<Ui::FlatLabel>(
			container,
			tr::ayu_AvatarCorners(),
			st::defaultSubsectionTitle);
		const auto titleRaw = title.data();

		const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				settings->avatarCornersValue() | rpl::map(mapRadius),
				st::settingsPremiumNewBadge),
			st::ayuBetaBadgePadding);
		badge->show();
		badge->setAttribute(Qt::WA_TransparentForMouseEvents);
		badge->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(badge);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			const auto r = st::ayuBetaBadgePadding.left();
			p.drawRoundedRect(badge->rect(), r, r);
		}, badge->lifetime());

		titleRaw->geometryValue() | rpl::on_next([=](QRect geometry) {
			badge->moveToLeft(
				geometry.x()
					+ titleRaw->textMaxWidth()
					+ st::settingsPremiumNewBadgePosition.x(),
				geometry.y()
					+ (geometry.height() - badge->height()) / 2);
		}, badge->lifetime());

		return {
			.widget = std::move(title),
			.margin = st::defaultSubsectionTitlePadding,
		};
	}, [] {
		return SearchEntry{
			.id = u"regram/ayu/avatarCorners"_q,
			.title = tr::ayu_AvatarCorners(tr::now),
		};
	});

	auto *previewRaw = static_cast<AvatarCornersPreview*>(nullptr);
	builder.add([&](const Builder::WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<AvatarCornersPreview>(
			ctx.container,
			controller);
		previewRaw = preview.data();
		const auto vMargin = st::settingsButtonNoIcon.padding
			- st::defaultDialogRow.padding;
		return {
			.widget = std::move(preview),
			.margin = QMargins(0, vMargin.top(), 0, vMargin.bottom()),
		};
	});

	ayu.addSlider({
		.id = u"regram/ayu/avatarCornersSlider"_q,
		.title = rpl::single(QString()),
		.showTitle = false,
		.steps = AyuUiSettings::kMaxAvatarCorners + 1,
		.current = settings->avatarCorners(),
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			if (previewRaw) {
				previewRaw->update();
			}
		},
		.onFinalChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			ShowRestartPrompt(controller);
		},
	});

	ayu.addSettingToggle({
		.id = u"regram/ayu/singleCornerRadius"_q,
		.title = tr::ayu_SingleCornerRadius(),
		.getter = &AyuSettings::singleCornerRadius,
		.setter = &AyuSettings::setSingleCornerRadius,
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SingleCornerRadiusDescription());
	builder.addSkip();
}

void BuildAppearance(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_CategoryAppearance());

	ayu.addSettingToggle({
		.id = u"regram/ayu/materialSwitches"_q,
		.altIds = { u"regram/ayu/newSwitchStyle"_q },
		.title = tr::ayu_MaterialSwitches(),
		.getter = &AyuSettings::materialSwitches,
		.setter = &AyuSettings::setMaterialSwitches,
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/disableCustomBackgrounds"_q,
		.altIds = { u"regram/ayu/customThemes"_q },
		.title = tr::ayu_DisableCustomBackgrounds(),
		.getter = &AyuSettings::disableCustomBackgrounds,
		.setter = &AyuSettings::setDisableCustomBackgrounds,
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/hidePremiumStatuses"_q,
		.title = tr::ayu_HidePremiumStatuses(),
		.getter = &AyuSettings::hidePremiumStatuses,
		.setter = &AyuSettings::setHidePremiumStatuses,
	});


	ayu.addSectionDivider();
}

void BuildTrayElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_TrayElementsHeader());

	ayu.addSettingToggle({
		.id = u"regram/ayu/showGhostToggleInTray"_q,
		.title = tr::ayu_EnableGhostModeTray(),
		.getter = &AyuSettings::showGhostToggleInTray,
		.setter = &AyuSettings::setShowGhostToggleInTray,
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"regram/ayu/showStreamerToggleInTray"_q,
		.title = tr::ayu_EnableStreamerModeTray(),
		.getter = &AyuSettings::showStreamerToggleInTray,
		.setter = &AyuSettings::setShowStreamerToggleInTray,
	});
#endif

	ayu.addSectionDivider();
}

void BuildDrawerElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_DrawerElementsHeader());

	ayu.addSettingToggle({
		.id = u"regram/ayu/showMyProfileInDrawer"_q,
		.title = tr::lng_menu_my_profile(),
		.getter = &AyuSettings::showMyProfileInDrawer,
		.setter = &AyuSettings::setShowMyProfileInDrawer,
		.icon = { &st::menuIconProfile },
	});

	const auto controller = builder.controller();
	if (controller && HasDrawerBots(controller)) {
		ayu.addSettingToggle({
			.id = u"regram/ayu/showBotsInDrawer"_q,
			.title = tr::lng_filters_type_bots(),
			.getter = &AyuSettings::showBotsInDrawer,
			.setter = &AyuSettings::setShowBotsInDrawer,
			.icon = { &st::menuIconBot },
		});
	}

	ayu.addSettingToggle({
		.id = u"regram/ayu/showNewGroupInDrawer"_q,
		.title = tr::lng_create_group_title(),
		.getter = &AyuSettings::showNewGroupInDrawer,
		.setter = &AyuSettings::setShowNewGroupInDrawer,
		.icon = { &st::menuIconGroups },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showNewChannelInDrawer"_q,
		.title = tr::lng_create_channel_title(),
		.getter = &AyuSettings::showNewChannelInDrawer,
		.setter = &AyuSettings::setShowNewChannelInDrawer,
		.icon = { &st::menuIconChannel },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showContactsInDrawer"_q,
		.title = tr::lng_menu_contacts(),
		.getter = &AyuSettings::showContactsInDrawer,
		.setter = &AyuSettings::setShowContactsInDrawer,
		.icon = { &st::menuIconUserShow },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showCallsInDrawer"_q,
		.title = tr::lng_menu_calls(),
		.getter = &AyuSettings::showCallsInDrawer,
		.setter = &AyuSettings::setShowCallsInDrawer,
		.icon = { &st::menuIconPhone },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showSavedMessagesInDrawer"_q,
		.title = tr::lng_saved_messages(),
		.getter = &AyuSettings::showSavedMessagesInDrawer,
		.setter = &AyuSettings::setShowSavedMessagesInDrawer,
		.icon = { &st::menuIconSavedMessages },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showLReadToggleInDrawer"_q,
		.title = tr::ayu_LReadMessages(),
		.getter = &AyuSettings::showLReadToggleInDrawer,
		.setter = &AyuSettings::setShowLReadToggleInDrawer,
		.icon = { &st::ayuLReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showSReadToggleInDrawer"_q,
		.title = tr::ayu_SReadMessages(),
		.getter = &AyuSettings::showSReadToggleInDrawer,
		.setter = &AyuSettings::setShowSReadToggleInDrawer,
		.icon = { &st::ayuSReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showNightModeToggleInDrawer"_q,
		.title = tr::lng_menu_night_mode(),
		.getter = &AyuSettings::showNightModeToggleInDrawer,
		.setter = &AyuSettings::setShowNightModeToggleInDrawer,
		.icon = { &st::menuIconNightMode },
	});
	ayu.addSettingToggle({
		.id = u"regram/ayu/showGhostToggleInDrawer"_q,
		.title = tr::ayu_GhostModeToggle(),
		.getter = &AyuSettings::showGhostToggleInDrawer,
		.setter = &AyuSettings::setShowGhostToggleInDrawer,
		.icon = { &st::ayuGhostIcon },
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"regram/ayu/showStreamerToggleInDrawer"_q,
		.title = tr::ayu_StreamerModeToggle(),
		.getter = &AyuSettings::showStreamerToggleInDrawer,
		.setter = &AyuSettings::setShowStreamerToggleInDrawer,
		.icon = { &st::ayuStreamerModeMenuIcon },
	});
#endif

	builder.addSkip();
}

} // namespace

void BuildExtraAppearance(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildAvatarCorners(builder, ayu);
	BuildAppearance(builder, ayu);
}

void BuildExtraMenu(SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildTrayElements(builder, ayu);
	BuildDrawerElements(builder, ayu);
}

} // namespace Settings
