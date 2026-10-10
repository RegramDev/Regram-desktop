#include "regram/nagram/links/inline_settings.h"

#include "regram/nagram/links/inline_rules.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram::Links {
namespace {

[[nodiscard]] std::optional<QJsonArray> Current() {
	const auto raw = ForDevice().Get(kInlineBotRules);
	if (!ValidateInlineRules(raw)) {
		return std::nullopt;
	}
	return (raw.isEmpty()
		? InlineDefaults()
		: QJsonDocument::fromJson(raw).object()).value(u"rules"_q).toArray();
}

[[nodiscard]] QByteArray Bytes(const QJsonArray &rules) {
	auto config = InlineDefaults();
	config.insert(u"rules"_q, rules);
	return rules.isEmpty()
		? QByteArray()
		: QJsonDocument(config).toJson(QJsonDocument::Compact);
}

bool Save(
		not_null<Ui::GenericBox*> box,
		const QJsonArray &before,
		const QJsonArray &after) {
	if (Current() != before) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	} else if (!ForDevice().Set(kInlineBotRules, Bytes(after))) {
		box->showToast(tr::lng_nagram_inline_invalid(tr::now));
		return false;
	}
	return true;
}

void TestBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_inline_test());

	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_link_sample()));
	field->setMaxLength(kMaxInlineLink);
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_inline_test_about(),
		st::boxLabel));
	result->setSelectable(true);
	box->addButton(tr::lng_nagram_inline_test(), [=] {
		const auto link = SingleLink(field->getLastText(), false);
		const auto match = link.isEmpty()
			? InlineMatch()
			: MatchInlineRules(
				CompileInlineRules(ForDevice().Get(kInlineBotRules)),
				link);
		result->setText(link.isEmpty()
			? tr::lng_nagram_inline_test_not_link(tr::now)
			: !match.error.isEmpty()
			? match.error
			: match.username.isEmpty()
			? tr::lng_nagram_inline_test_none(tr::now)
			: tr::lng_nagram_inline_test_match(
				tr::now,
				lt_username,
				match.username));
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void RuleBox(
		not_null<Ui::GenericBox*> box,
		QJsonArray rules,
		int index) {
	box->setTitle(tr::lng_nagram_inline_rule());

	const auto rule = (index < rules.size())
		? rules[index].toObject()
		: NewInlineRule(QString(), {});
	auto lines = QStringList();
	for (const auto &pattern : rule.value(u"patterns"_q).toArray()) {
		lines.push_back(pattern.toString());
	}
	const auto username = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_inline_username(),
		rule.value(u"username"_q).toString()));
	username->setMaxLength(33);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_inline_patterns(),
		st::boxLabel));
	const auto patterns = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::MultiLine,
		rpl::single(QString()),
		lines.join('\n')));
	const auto enabled = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_nagram_inline_rule_enabled(tr::now),
		rule.value(u"enabled"_q).toBool()));
	box->addButton(tr::lng_settings_save(), [=] {
		auto name = username->getLastText().trimmed();
		if (name.startsWith(u'@')) {
			name.remove(0, 1);
		}
		const auto list = patterns->getLastText().split(
			u'\n',
			Qt::SkipEmptyParts);
		if (!ValidInlineUsername(name)) {
			box->showToast(tr::lng_nagram_inline_invalid(tr::now));
			return;
		} else if (const auto problem = CheckInlinePatterns(list)) {
			box->showToast(tr::lng_nagram_inline_pattern_error(
				tr::now,
				lt_index,
				QString::number(problem->line),
				lt_error,
				problem->text));
			return;
		}
		auto changed = rule;
		changed.insert(u"username"_q, name);
		changed.insert(u"patterns"_q, QJsonArray::fromStringList(list));
		changed.insert(u"enabled"_q, enabled->checked());
		auto updated = rules;
		if (index < updated.size()) {
			updated[index] = changed;
		} else {
			updated.push_back(changed);
		}
		if (Save(box, rules, updated)) {
			box->closeBox();
		}
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void InlineRulesBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_inline_rules());

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_inline_auto_about(),
		st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto show = [=](object_ptr<Ui::BoxContent> content) {
		box->getDelegate()->show(
			std::move(content),
			Ui::LayerOption::KeepOther);
	};
	const auto refresh = [=] {
		rows->clear();
		const auto rules = Current();
		if (!rules) {
			rows->add(object_ptr<Ui::FlatLabel>(
				rows,
				tr::lng_nagram_inline_invalid(),
				st::boxLabel));
			return;
		}
		for (auto index = 0; index < rules->size(); ++index) {
			const auto rule = (*rules)[index].toObject();
			const auto button = rows->add(object_ptr<Ui::SettingsButton>(
				rows,
				rpl::single(u'@' + rule.value(u"username"_q).toString()
					+ u" · "_q
					+ rule.value(u"patterns"_q).toArray().first().toString()
					+ (rule.value(u"enabled"_q).toBool()
						? QString()
						: u" · "_q + tr::lng_nagram_config_off(tr::now))),
				st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				const auto menu = Ui::CreateChild<Ui::PopupMenu>(box);
				menu->addAction(tr::lng_nagram_inline_rule(tr::now), [=] {
					show(Box(RuleBox, *rules, index));
				});
				for (const auto &delta : { -1, 1 }) {
					if (index + delta < 0 || index + delta >= rules->size()) {
						continue;
					}
					menu->addAction((delta < 0)
						? tr::lng_link_move_up(tr::now)
						: tr::lng_link_move_down(tr::now), [=] {
						auto ordered = *rules;
						const auto entry = ordered.takeAt(index);
						ordered.insert(index + delta, entry);
						Save(box, *rules, ordered);
					});
				}
				menu->addAction(tr::lng_box_delete(tr::now), [=] {
					auto updated = *rules;
					updated.removeAt(index);
					Save(box, *rules, updated);
				});
				menu->popup(QCursor::pos());
			});
		}
	};
	ForDevice().changes() | rpl::on_next([=](std::string_view) {
		InvokeQueued(box, refresh);
	}, box->lifetime());
	box->addButton(tr::lng_nagram_inline_add(), [=] {
		if (const auto rules = Current()) {
			show(Box(RuleBox, *rules, int(rules->size())));
		}
	});
	box->addButton(tr::lng_nagram_inline_test(), [=] {
		show(Box(TestBox));
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	refresh();
}

} // namespace Nagram::Links
