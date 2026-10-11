#include "regram/nagram/settings/privacy.h"

#include "regram/nagram/privacy/options.h"
#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/restart.h"
#include "api/api_sensitive_content.h"
#include "apiwrap.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/layers/generic_box.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class PrivacySection final : public Section<PrivacySection> {
public:
	PrivacySection(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_privacy();
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

QString ProfileIdFormatLabel(int format) {
	return (format == 1)
		? tr::lng_nagram_id_bot_api(tr::now)
		: (format == 2)
		? tr::lng_nagram_id_raw(tr::now)
		: tr::lng_nagram_id_off(tr::now);
}

std::vector<QString> NameOrderLabels() {
	return {
		tr::lng_nagram_preview_follow(tr::now),
		tr::lng_nagram_name_order_first(tr::now),
		tr::lng_nagram_name_order_last(tr::now),
	};
}

std::vector<QString> PersianCalendarLabels() {
	return {
		tr::lng_nagram_reading_off(tr::now),
		tr::lng_nagram_persian_calendar_native(tr::now),
		tr::lng_nagram_persian_calendar_latin(tr::now),
	};
}

void ChoiceBox(
		not_null<Ui::GenericBox*> box,
		const Option<int> *option,
		const tr::phrase<> *title,
		std::vector<QString> labels,
		Window::SessionController *restart) {
	box->setTitle((*title)());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(*option));
	for (auto value = 0; value != int(labels.size()); ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, labels[value],
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (ForDevice().Get(*option) != value) {
			Expects(ForDevice().Set(*option, value));
			if (restart) {
				ShowRestartPrompt(restart);
			}
		}
		box->closeBox();
	});
}

void ModerateDefaultsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_moderate_defaults());
	const auto current = ForDevice().Get(Privacy::kModerateDefaults);
	const auto labels = std::array{
		tr::lng_report_spam(tr::now),
		tr::lng_nagram_moderate_delete_all(tr::now),
		tr::lng_nagram_moderate_ban(tr::now),
	};
	auto checks = std::vector<not_null<Ui::Checkbox*>>();
	for (auto i = 0; i != int(labels.size()); ++i) {
		checks.push_back(box->addRow(object_ptr<Ui::Checkbox>(
			box, labels[i], (current & (1 << i)) != 0)));
	}
	box->addButton(tr::lng_settings_save(), [=] {
		auto value = 0;
		for (auto i = 0; i != int(checks.size()); ++i) {
			value |= checks[i]->checked() ? (1 << i) : 0;
		}
		Expects(ForDevice().Set(Privacy::kModerateDefaults, value));
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ProfileIdFormatBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_profile_id_format());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Privacy::kProfileIdFormat));
	for (auto value = 0; value != 3; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, ProfileIdFormatLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Privacy::kProfileIdFormat, value));
		box->closeBox();
	});
}

const auto kMeta = BuildHelper({
	.id = PrivacySection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_privacy,
	.icon = &st::menuIconLock,
}, [](SectionBuilder &builder) {
	const auto session = builder.session();
	const auto button = builder.addButton({
		.id = u"regram/nagram/privacy/hide-my-phone"_q,
		.title = tr::lng_nagram_hide_my_phone(),
		.st = &st::settingsButtonNoIcon,
		.toggled = session->settings().phoneNumberHiddenValue(),
		.keywords = { u"phone"_q, u"number"_q },
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([=](bool value) {
			session->settings().setPhoneNumberHidden(value);
			session->saveSettingsDelayed();
		}, button->lifetime());
	}
	AddToggle(builder, Privacy::kDemoMode,
		tr::lng_nagram_demo_mode(),
		u"regram/nagram/privacy/demo-mode"_q,
		{ u"presentation"_q, u"capture"_q });
	builder.addDividerText(tr::lng_nagram_demo_mode_note());
	AddToggle(builder, Privacy::kHideReadTime,
		tr::lng_nagram_hide_read_time(),
		u"regram/nagram/privacy/hide-read-time"_q,
		{ u"read"_q, u"time"_q });
	AddToggle(builder, Privacy::kHideSharePhonePrompt,
		tr::lng_nagram_hide_share_phone_prompt(),
		u"regram/nagram/privacy/hide-share-phone-prompt"_q,
		{ u"share"_q, u"phone"_q });
	AddToggle(builder, Privacy::kDoNotSharePhone,
		tr::lng_nagram_do_not_share_phone(),
		u"regram/nagram/privacy/do-not-share-phone"_q,
		{ u"share"_q, u"phone"_q, u"contact"_q });
	builder.addDividerText(tr::lng_nagram_do_not_share_phone_about());
	AddToggle(builder, Privacy::kForceCopy,
		tr::lng_nagram_force_copy(),
		u"regram/nagram/privacy/force-copy"_q,
		{ u"copy"_q, u"save"_q, u"protected"_q, u"forward"_q });
	builder.addDividerText(tr::lng_nagram_force_copy_about());
	AddToggle(builder, Privacy::kIgnoreContentRestrictions,
		tr::lng_nagram_ignore_restrictions(),
		u"regram/nagram/privacy/ignore-restrictions"_q,
		{ u"restriction"_q, u"unavailable"_q });
	builder.addDividerText(tr::lng_nagram_ignore_restrictions_about());
	const auto sensitive = &session->api().sensitiveContent();
	if (builder.controller()) {
		sensitive->reload();
	}
	if (builder.controller() || sensitive->canChangeCurrent()) {
		builder.scope([&] {
			AddToggle(builder, Privacy::kSkipSensitiveWarning,
				tr::lng_nagram_skip_sensitive_warning(),
				u"regram/nagram/privacy/skip-sensitive-warning"_q,
				{ u"sensitive"_q, u"18+"_q, u"spoiler"_q });
			builder.addDividerText(
				tr::lng_nagram_skip_sensitive_warning_about());
		}, sensitive->canChange());
	}
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/privacy/profile-id-format"_q,
		.title = tr::lng_nagram_profile_id_format(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Privacy::kProfileIdFormat)
			| rpl::map(ProfileIdFormatLabel),
		.onClick = [=] { controller->show(Box(ProfileIdFormatBox)); },
		.keywords = { u"profile"_q, u"ID"_q },
	});
	AddToggle(builder, Privacy::kShowProfileDc,
		tr::lng_nagram_show_profile_dc(),
		u"regram/nagram/privacy/show-profile-dc"_q,
		{ u"profile"_q, u"DC"_q });
	AddToggle(builder, Privacy::kShowRegistrationDate,
		tr::lng_nagram_show_registration_date(),
		u"regram/nagram/privacy/show-registration-date"_q,
		{ u"profile"_q, u"registration"_q, u"date"_q });
	builder.addDividerText(tr::lng_nagram_show_registration_date_about());
	AddToggle(builder, Privacy::kAntiAutoDelete,
		tr::lng_regram_anti_auto_delete(), u"regram/privacy/anti-auto-delete"_q,
		{ u"auto delete"_q, u"TTL"_q });
	AddToggle(builder, Privacy::kAntiSelfDestruct,
		tr::lng_regram_anti_self_destruct(), u"regram/privacy/anti-self-destruct"_q,
		{ u"self destruct"_q, u"media"_q });
	AddToggle(builder, Privacy::kProfileCommonGroupsFirst,
		tr::lng_regram_profile_common_groups_first(),
		u"regram/privacy/common-groups-first"_q,
		{ u"profile"_q, u"common groups"_q, u"bot"_q });
	AddToggle(builder, Privacy::kHideProfileGifts,
		tr::lng_nagram_hide_profile_gifts(),
		u"regram/nagram/privacy/hide-profile-gifts"_q,
		{ u"profile"_q, u"gifts"_q });
	AddToggle(builder, Privacy::kHideCreateTodo,
		tr::lng_nagram_hide_create_todo(),
		u"regram/nagram/privacy/hide-create-todo"_q,
		{ u"todo"_q, u"list"_q });
	AddToggle(builder, Privacy::kAdminShortcuts,
		tr::lng_nagram_admin_shortcuts_option(),
		u"regram/nagram/privacy/admin-shortcuts"_q,
		{ u"admin"_q, u"manage"_q, u"permissions"_q });
	const auto addChoice = [&](
			const Option<int> *option,
			const tr::phrase<> *title,
			std::vector<QString> (*labels)(),
			bool restart,
			QString id,
			QStringList keywords) {
		builder.addButton({
			.id = std::move(id),
			.title = (*title)(),
			.st = &st::settingsButtonNoIcon,
			.label = ForDevice().Value(*option)
				| rpl::map([=](int value) { return labels().at(value); }),
			.onClick = [=] {
				controller->show(Box(
					ChoiceBox,
					option,
					title,
					labels(),
					restart ? controller : nullptr));
			},
			.keywords = std::move(keywords),
		});
	};
	addChoice(&Privacy::kNameOrder,
		&tr::lng_nagram_name_order,
		NameOrderLabels,
		true,
		u"regram/nagram/privacy/name-order"_q,
		{ u"name"_q, u"order"_q, u"last name"_q });
	addChoice(&Privacy::kPersianCalendar,
		&tr::lng_nagram_persian_calendar,
		PersianCalendarLabels,
		false,
		u"regram/nagram/privacy/persian-calendar"_q,
		{ u"Persian"_q, u"Jalali"_q, u"calendar"_q });
	builder.addButton({
		.id = u"regram/nagram/privacy/moderate-defaults"_q,
		.title = tr::lng_nagram_moderate_defaults(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Box(ModerateDefaultsBox)); },
		.keywords = { u"delete"_q, u"ban"_q, u"report"_q },
	});
});

const SectionBuildMethod PrivacySection::kBuild = kMeta.build;

} // namespace

Settings::Type PrivacyId() {
	return PrivacySection::Id();
}

} // namespace Nagram
