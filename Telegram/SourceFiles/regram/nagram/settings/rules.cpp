#include "regram/nagram/settings/rules.h"

#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/filters/settings.h"
#include "regram/nagram/settings/home.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class RulesSection final : public Section<RulesSection> {
public:
	RulesSection(QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

	static const SectionBuildMethod kBuild;

};

RulesSection::RulesSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kBuild);
	Ui::ResizeFitChild(this, content);
}

rpl::producer<QString> RulesSection::title() {
	return tr::lng_regram_ios_filters();
}

const auto kMeta = BuildHelper({
	.id = RulesSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_regram_ios_filters,
	.icon = &st::menuIconTagFilter,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/rules/ios-filters"_q,
		.title = tr::lng_regram_ios_filters(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Regram::Filters::SettingsBox, &controller->session()));
		},
		.keywords = { u"filter"_q, u"regex"_q, u"block"_q },
	});
});

const SectionBuildMethod RulesSection::kBuild = kMeta.build;

} // namespace

Settings::Type RulesId() {
	return RulesSection::Id();
}

} // namespace Nagram
