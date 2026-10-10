#include "regram/nagram/settings/rules.h"

#include "regram/nagram/settings/home.h"
#include "regram/nagram/filters/settings.h"
#include "regram/filters/settings.h"
#include "regram/nagram/filters/hidden_messages.h"
#include "regram/nagram/filters/model.h"
#include "regram/nagram/notifications/settings.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/links/inline_rules.h"
#include "regram/nagram/links/inline_settings.h"
#include "regram/nagram/links/webview.h"
#include "regram/nagram/links/settings.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "main/main_session.h"
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
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_rules();
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

std::vector<QString> HashtagPageLabels() {
	return {
		tr::lng_nagram_preview_follow(tr::now),
		tr::lng_nagram_hashtag_page_this_chat(tr::now),
		tr::lng_nagram_hashtag_page_my_messages(tr::now),
	};
}

void ChoiceBox(
		not_null<Ui::GenericBox*> box,
		const Option<int> *option,
		const tr::phrase<> *title,
		std::vector<int> values,
		std::vector<QString> labels) {
	box->setTitle((*title)());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(*option));
	for (auto i = 0; i != int(values.size()); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, values[i], labels[i],
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(*option, value));
		box->closeBox();
	});
}

void WebviewPatternBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_webview_external());
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_filter_pattern(),
		ForDevice().Get(Links::kWebviewExternalPattern)));
	field->setMaxLength(Links::kMaxWebviewPattern);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		const auto value = field->getLastText();
		if (const auto problem = Links::CheckWebviewPattern(value)) {
			field->showError();
			box->showToast(tr::lng_nagram_webview_external_invalid(
				tr::now,
				lt_index,
				QString::number(problem->position),
				lt_error,
				problem->text));
			return;
		}
		Expects(ForDevice().Set(Links::kWebviewExternalPattern, value));
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = RulesSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_rules,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/rules/ios-filters"_q,
		.title = tr::lng_regram_ios_filters(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Regram::Filters::SettingsBox, &controller->session()));
		},
		.keywords = { u"iOS"_q, u"filter"_q, u"regex"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/rules/messages"_q,
		.title = tr::lng_nagram_filters(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Filters::SettingsBox, &controller->session()));
		},
		.keywords = { u"filter"_q, u"regex"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/rules/links"_q,
		.title = tr::lng_nagram_link_rules(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Links::SettingsBox));
		},
		.keywords = { u"link"_q, u"URL"_q },
	});
	const auto session = builder.session();
	builder.addButton({
		.id = u"regram/nagram/rules/hidden-messages"_q,
		.title = tr::lng_nagram_hidden_messages(),
		.st = &st::settingsButtonNoIcon,
		.label = ForAccount(session).Value(Filters::kHiddenMessages)
			| rpl::map([=](const QString &) {
				return QString::number(Filters::HiddenMessagesCount(session));
			}),
		.onClick = [=] {
			if (!Filters::HiddenMessagesCount(session)) {
				return;
			}
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_hidden_messages_clear(),
				.confirmed = [=](Fn<void()> &&close) {
					Filters::ClearHiddenMessages(session);
					close();
				},
			}));
		},
		.keywords = { u"hide"_q, u"message"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/rules/filters-global"_q,
		.title = tr::lng_nagram_filter_global(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Filters::GlobalSettingsBox, session));
		},
		.keywords = { u"filter"_q, u"regex"_q, u"global"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/rules/filter-scopes"_q,
		.title = tr::lng_nagram_filter_scopes(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Filters::ScopesSettingsBox, session));
		},
		.keywords = { u"filter"_q, u"chat"_q, u"topic"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/rules/keyword-alerts"_q,
		.title = tr::lng_nagram_keyword_alerts(),
		.st = &st::settingsButtonNoIcon,
		.label = Notifications::KeywordAlertsLabel(session),
		.onClick = [=] {
			controller->show(Box(Notifications::KeywordAlertsBox, session));
		},
		.keywords = { u"keyword"_q, u"notification"_q, u"alert"_q },
	});
	AddToggle(builder, Links::kAutoInlineBot,
		tr::lng_nagram_inline_auto(),
		u"regram/nagram/rules/inline-auto"_q,
		{ u"inline"_q, u"bot"_q, u"link"_q });
	builder.addButton({
		.id = u"regram/nagram/rules/inline-rules"_q,
		.title = tr::lng_nagram_inline_rules(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			controller->show(Box(Links::InlineRulesBox));
		},
		.keywords = { u"inline"_q, u"bot"_q, u"regex"_q },
	});
	builder.addDividerText(tr::lng_nagram_inline_auto_about());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/rules/links-search"_q,
		.title = tr::lng_nagram_links_search_group(),
		.keywords = { u"link"_q, u"search"_q },
	});
	AddToggle(builder, Links::kDisableOfficialAutoLogin,
		tr::lng_nagram_disable_official_auto_login(),
		u"regram/nagram/rules/disable-official-auto-login"_q,
		{ u"login"_q, u"website"_q, u"token"_q });
	builder.addDividerText(
		tr::lng_nagram_disable_official_auto_login_about());
	const auto addHashtagPage = [&](
			const Option<int> *option,
			const tr::phrase<> *title,
			QString id) {
		builder.addButton({
			.id = std::move(id),
			.title = (*title)(),
			.st = &st::settingsButtonNoIcon,
			.label = ForDevice().Value(*option) | rpl::map([](int value) {
				return HashtagPageLabels().at(value);
			}),
			.onClick = [=] {
				controller->show(Box(
					ChoiceBox,
					option,
					title,
					std::vector<int>{ 0, 1, 2 },
					HashtagPageLabels()));
			},
			.keywords = { u"hashtag"_q, u"cashtag"_q, u"search"_q },
		});
	};
	addHashtagPage(&Links::kHashtagSearchPageChannel,
		&tr::lng_nagram_hashtag_page_channel,
		u"regram/nagram/rules/hashtag-page-channel"_q);
	addHashtagPage(&Links::kHashtagSearchPageChat,
		&tr::lng_nagram_hashtag_page_chat,
		u"regram/nagram/rules/hashtag-page-chat"_q);
	builder.addSubsectionTitle({
		.id = u"regram/nagram/rules/web-app"_q,
		.title = tr::lng_nagram_web_app_group(),
		.keywords = { u"web app"_q, u"mini app"_q },
	});
	const auto addWebAppScale = [&](
			const Option<int> *option,
			const tr::phrase<> *title,
			QString id) {
		builder.addButton({
			.id = std::move(id),
			.title = (*title)(),
			.st = &st::settingsButtonNoIcon,
			.label = ForDevice().Value(*option) | rpl::map([](int value) {
				return QString::number(value) + '%';
			}),
			.onClick = [=] {
				const auto values = std::vector<int>{ 100, 125, 150, 175, 200 };
				auto labels = std::vector<QString>();
				for (const auto &value : values) {
					labels.push_back(QString::number(value) + '%');
				}
				controller->show(Box(
					ChoiceBox,
					option,
					title,
					values,
					std::move(labels)));
			},
			.keywords = { u"web app"_q, u"mini app"_q, u"size"_q },
		});
	};
	addWebAppScale(&Links::kWebAppWidthScale,
		&tr::lng_nagram_web_app_width,
		u"regram/nagram/rules/web-app-width"_q);
	addWebAppScale(&Links::kWebAppHeightScale,
		&tr::lng_nagram_web_app_height,
		u"regram/nagram/rules/web-app-height"_q);
	builder.addDividerText(tr::lng_nagram_web_app_size_about());
	AddToggle(builder, Links::kWebAppAndroidPlatform,
		tr::lng_nagram_web_app_android_platform(),
		u"regram/nagram/rules/web-app-platform"_q,
		{ u"web app"_q, u"mini app"_q, u"platform"_q, u"Android"_q });
	builder.addDividerText(tr::lng_nagram_web_app_android_platform_about());
	builder.addButton({
		.id = u"regram/nagram/rules/web-app-external"_q,
		.title = tr::lng_nagram_webview_external(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Links::kWebviewExternalPattern),
		.onClick = [=] {
			controller->show(Box(WebviewPatternBox));
		},
		.keywords = { u"web app"_q, u"mini app"_q, u"browser"_q, u"regex"_q },
	});
	builder.addDividerText(tr::lng_nagram_webview_external_about());
});

const SectionBuildMethod RulesSection::kBuild = kMeta.build;

} // namespace

Settings::Type RulesId() {
	return RulesSection::Id();
}

} // namespace Nagram
