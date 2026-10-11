#include "regram/nagram/notifications/settings.h"

#include "regram/nagram/notifications/model.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "styles/style_settings.h"

#include <QtCore/QLocale>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram::Notifications {
namespace {

[[nodiscard]] style::margins RowMargin(int top) {
	return style::margins(
		st::boxRowPadding.left(),
		top,
		st::boxRowPadding.right(),
		0);
}

[[nodiscard]] not_null<Ui::Checkbox*> AddCheck(
		not_null<Ui::GenericBox*> box,
		const QString &text,
		bool checked) {
	return box->addRow(
		object_ptr<Ui::Checkbox>(box, text, checked),
		RowMargin(st::boxLittleSkip));
}

void AddTitle(not_null<Ui::GenericBox*> box, rpl::producer<QString> text) {
	box->addRow(
		object_ptr<Ui::FlatLabel>(box, std::move(text), st::boxLabel),
		RowMargin(st::boxMediumSkip));
}

} // namespace

QString PolicyLabel(const QString &policy) {
	return policy == u"silenced"_q ? tr::lng_regram_notification_silenced(tr::now)
		: policy == u"disabled"_q ? tr::lng_nagram_config_off(tr::now)
		: tr::lng_nagram_preview_follow(tr::now);
}

void PolicyBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		bool pinned,
		uint64 peer) {
	const auto option = pinned ? kPinnedPolicy : kMentionPolicy;
	const auto exception = pinned ? kPinnedExceptions : kMentionExceptions;
	const auto key = QString::number(peer);
	auto &options = ForAccount(session);
	const auto raw = options.Get(exception);
	const auto entry = QJsonDocument::fromJson(raw).object().value(key).toString();
	const auto policies = QStringList{ u"default"_q, u"silenced"_q, u"disabled"_q };
	const auto chosen = peer ? policies.indexOf(entry) : policies.indexOf(options.Get(option));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(int(chosen));
	box->setTitle(pinned ? tr::lng_regram_pinned_notifications() : tr::lng_regram_mention_notifications());
	if (peer) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, -1, tr::lng_regram_notification_inherit(tr::now),
			st::settingsSendType));
	}
	for (auto i = 0; i != policies.size(); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, i, PolicyLabel(policies[i]), st::settingsSendType));
	}
	group->setChangedCallback(crl::guard(session, [=](int value) {
		auto &options = ForAccount(session);
		if (!peer) {
			Expects(options.Set(option, policies[value]));
		} else {
			auto values = QJsonDocument::fromJson(options.Get(exception)).object();
			if (value < 0) {
				values.remove(key);
			} else {
				values.insert(key, policies[value]);
			}
			Expects(options.Set(exception, QJsonDocument(values).toJson(QJsonDocument::Compact)));
		}
		box->closeBox();
	}));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

rpl::producer<QString> QuietHoursLabel() {
	return ForDevice().Value(kQuietHours) | rpl::map([](const QByteArray &raw) {
		const auto config = ParseQuietHours(raw).value_or(QuietHours());
		return config.enabled
			? (FormatMinute(config.start) + u" – "_q + FormatMinute(config.end))
			: tr::lng_nagram_config_off(tr::now);
	});
}

rpl::producer<QString> KeywordAlertsLabel(not_null<Main::Session*> session) {
	return ForAccount(session).Value(
		kKeywordAlerts
	) | rpl::map([](const QByteArray &raw) {
		const auto config = ParseKeywordAlerts(raw).value_or(KeywordAlerts());
		return config.enabled
			? QString::number(config.rules.size())
			: tr::lng_nagram_config_off(tr::now);
	});
}

void QuietHoursBox(not_null<Ui::GenericBox*> box) {
	const auto config = ParseQuietHours(
		ForDevice().Get(kQuietHours)).value_or(QuietHours());
	box->setTitle(tr::lng_nagram_quiet_hours());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_quiet_hours_about(),
		st::boxDividerLabel));
	const auto enabled = AddCheck(
		box,
		tr::lng_nagram_quiet_hours_enable(tr::now),
		config.enabled);
	const auto addTime = [&](rpl::producer<QString> title, int minute) {
		const auto field = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				std::move(title),
				FormatMinute(minute)),
			RowMargin(st::boxLittleSkip));
		field->setMaxLength(5);
		return field;
	};
	const auto start = addTime(tr::lng_nagram_quiet_hours_start(), config.start);
	const auto end = addTime(tr::lng_nagram_quiet_hours_end(), config.end);
	AddTitle(box, tr::lng_nagram_quiet_hours_days());
	auto days = std::vector<not_null<Ui::Checkbox*>>();
	const auto locale = QLocale();
	for (auto day = 1; day <= 7; ++day) {
		const auto chosen = config.weekdays.empty()
			|| (std::ranges::find(config.weekdays, day)
				!= config.weekdays.end());
		days.push_back(AddCheck(box, locale.standaloneDayName(day), chosen));
	}
	AddTitle(box, tr::lng_nagram_quiet_hours_exceptions());
	const auto contacts = AddCheck(
		box,
		tr::lng_nagram_quiet_hours_contacts(tr::now),
		config.allowContacts);
	const auto pinned = AddCheck(
		box,
		tr::lng_nagram_quiet_hours_pinned(tr::now),
		config.allowPinned);
	const auto mentions = AddCheck(
		box,
		tr::lng_nagram_quiet_hours_mentions(tr::now),
		config.allowMentions);
	const auto keywords = AddCheck(
		box,
		tr::lng_nagram_quiet_hours_keywords(tr::now),
		config.allowKeywords);
	const auto save = [=] {
		const auto from = ParseMinute(start->getLastText());
		const auto till = ParseMinute(end->getLastText());
		if (!from || !till) {
			(from ? end : start)->showError();
			box->showToast(tr::lng_nagram_quiet_hours_bad_time(tr::now));
			return;
		}
		auto weekdays = std::vector<int>();
		for (auto day = 1; day <= 7; ++day) {
			if (days[day - 1]->checked()) {
				weekdays.push_back(day);
			}
		}
		if (weekdays.empty()) {
			box->showToast(tr::lng_nagram_quiet_hours_no_days(tr::now));
			return;
		} else if (weekdays.size() == 7) {
			weekdays.clear();
		}
		const auto saved = ForDevice().Set(kQuietHours, Serialize(QuietHours{
			.enabled = enabled->checked(),
			.start = *from,
			.end = *till,
			.weekdays = std::move(weekdays),
			.allowContacts = contacts->checked(),
			.allowPinned = pinned->checked(),
			.allowMentions = mentions->checked(),
			.allowKeywords = keywords->checked(),
		}));
		if (saved) {
			box->closeBox();
		}
	};
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void KeywordAlertsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	const auto config = ParseKeywordAlerts(
		ForAccount(session).Get(kKeywordAlerts)).value_or(KeywordAlerts());
	box->setTitle(tr::lng_nagram_keyword_alerts());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_keyword_alerts_about(),
		st::boxDividerLabel));
	const auto enabled = AddCheck(
		box,
		tr::lng_nagram_keyword_alerts_enable(tr::now),
		config.enabled);
	const auto channels = AddCheck(
		box,
		tr::lng_nagram_keyword_alerts_channels(tr::now),
		config.channels);
	const auto field = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine,
			tr::lng_nagram_keyword_alerts_field(),
			FormatKeywordLines(config.rules)),
		RowMargin(st::boxMediumSkip));
	field->setMaxLength(kMaxKeywordRules * (kMaxKeywordLength + 4));
	const auto save = crl::guard(session, [=] {
		auto rules = ParseKeywordLines(field->getLastText());
		if (int(rules.size()) > kMaxKeywordRules) {
			box->showToast(tr::lng_nagram_keyword_alerts_too_many(
				tr::now,
				lt_amount,
				QString::number(kMaxKeywordRules)));
			return;
		} else if (const auto problem = CheckKeywordRules(rules)) {
			field->showError();
			box->showToast(tr::lng_nagram_keyword_alerts_invalid(
				tr::now,
				lt_index,
				QString::number(problem->line),
				lt_error,
				problem->text.isEmpty()
					? tr::lng_nagram_keyword_alerts_too_long(tr::now)
					: problem->text));
			return;
		}
		const auto saved = ForAccount(session).Set(
			kKeywordAlerts,
			Serialize(KeywordAlerts{
				.enabled = enabled->checked(),
				.channels = channels->checked(),
				.rules = std::move(rules),
			}));
		if (saved) {
			box->closeBox();
		}
	});
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace Nagram::Notifications
