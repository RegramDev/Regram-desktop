// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/ui/settings/settings_main.h"

#include "regram/nagram/settings/home.h"
#include "settings/sections/settings_main.h"
#include "lang_auto.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/ayu/ui/ayu_logo.h"
#include "regram/ayu/ui/settings/settings_appearance.h"
#include "regram/ayu/ui/settings/settings_ayu.h"
#include "regram/ayu/ui/settings/settings_chats.h"
#include "regram/ayu/ui/settings/settings_general.h"
#include "regram/ayu/ui/settings/settings_other.h"
#include "core/version.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QDesktopServices>

namespace Settings {

using namespace Builder;

namespace {

void BuildLogo(SectionBuilder &builder) {
	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto logo = object_ptr<Ui::RpWidget>(ctx.container);
		const auto logoRaw = logo.data();
		logoRaw->resize(
			QSize(st::settingsCloudPasswordIconSize,
				st::settingsCloudPasswordIconSize));
		logoRaw->setNaturalWidth(st::settingsCloudPasswordIconSize);
		struct State {
			QImage scaled;
			qint64 key = 0;
			qreal ratio = 0.;
		};
		const auto state = logoRaw->lifetime().make_state<State>();
		logoRaw->paintRequest(
		) | rpl::on_next([=] {
			auto p = QPainter(logoRaw);
			const auto image = AyuAssets::currentAppLogoPad();
			if (!image.isNull()) {
				const auto size = st::settingsCloudPasswordIconSize;
				const auto ratio = style::DevicePixelRatio();
				if (state->key != image.cacheKey() || state->ratio != ratio) {
					state->key = image.cacheKey();
					state->ratio = ratio;
					state->scaled = image.scaled(size * ratio, size * ratio,
						Qt::KeepAspectRatio, Qt::SmoothTransformation);
				}
				p.drawImage(QRect(0, 0, size, size), state->scaled);
			}
		}, logoRaw->lifetime());
		return { .widget = std::move(logo), .align = style::al_top };
	});
}

void BuildVersionInfo(SectionBuilder &builder) {
	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<Ui::FlatLabel>(
				ctx.container,
				rpl::single(
					u"Regram Desktop v"_q
					+ QString::fromLatin1(AppVersionStr)),
				st::boxTitle),
			.align = style::al_top,
		};
	});

	builder.addSkip();

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<Ui::FlatLabel>(
				ctx.container,
				tr::ayu_SettingsDescription(),
				st::centeredBoxLabel),
			.align = style::al_top,
		};
	});
}

void BuildCategories(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSkip();
	builder.addSkip();
	builder.addSkip();
	builder.addDivider();
	builder.addSkip();

	builder.addSubsectionTitle(tr::ayu_CategoriesHeader());

	builder.addSectionButton({
		.title = tr::ayu_GhostModeToggle(),
		.targetSection = AyuGhost::Id(),
		.icon = { &st::menuIconGroupReactions },
	});
	Nagram::AddCategoryEntries(builder);
	builder.addSectionButton({
		.title = tr::lng_regram_advanced_tools(),
		.targetSection = AyuOther::Id(),
		.icon = { &st::menuIconFave },
	});
}

void BuildLinks(SectionBuilder &builder) {
	builder.addSkip();
	builder.addDivider();
	builder.addSkip();
	builder.addButton({
		.id = u"regram/source"_q,
		.title = tr::lng_nagram_source_code(),
		.icon = { &st::menuIconLink },
		.label = rpl::single(u"Regram"_q),
		.onClick = [] {
			QDesktopServices::openUrl(
				u"https://github.com/RegramDev/Regram-desktop"_q);
		},
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuMain::Id(),
	.parentId = MainId(),
	.title = &tr::lng_regram_settings,
	.icon = &st::menuIconPremium,
}, [](SectionBuilder &builder) {
	BuildLogo(builder);
	builder.addSkip();
	BuildVersionInfo(builder);
	BuildCategories(builder);
	BuildLinks(builder);
});

} // namespace

rpl::producer<QString> AyuMain::title() {
	return tr::lng_regram_settings();
}

AyuMain::AyuMain(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuMain::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuMainId() {
	return AyuMain::Id();
}

} // namespace Settings
