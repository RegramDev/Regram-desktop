#include "regram/nagram/links/settings.h"
#include "regram/nagram/links/model.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "regram/nagram/core/options.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>
#include <optional>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram::Links {
namespace {

std::optional<QJsonObject> Current() {
	const auto raw = ForDevice().Get(kRules);
	return Validate(raw)
		? std::make_optional(raw.isEmpty()
			? Defaults() : QJsonDocument::fromJson(raw).object())
		: std::nullopt;
}

bool Valid(const QJsonObject &config) {
	return Validate(QJsonDocument(config).toJson(QJsonDocument::Compact));
}


bool Save(
		not_null<Ui::GenericBox*> box,
		const QJsonObject &before,
		const QJsonObject &after) {
	if (Current() != before) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	} else if (!Valid(after)) {
		box->showToast(tr::lng_nagram_link_invalid(tr::now));
		return false;
	}
	Expects(ForDevice().Set(kRules,
		after == Defaults() ? QByteArray()
			: QJsonDocument(after).toJson(QJsonDocument::Compact)));
	return true;
}

void PreviewBox(not_null<Ui::GenericBox*> box, QJsonObject config) {
	box->setTitle(tr::lng_nagram_link_preview());

	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_link_sample()));
	field->setMaxLength(16384);
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_link_preview_about(), st::boxLabel));
	result->setSelectable(true);
	box->addButton(tr::lng_nagram_link_preview(), [=] {
		const auto rewritten = Rewrite(
			QJsonDocument(config).toJson(QJsonDocument::Compact),
			field->getLastText());
		result->setText(rewritten.error.isEmpty()
			? rewritten.url.toString(QUrl::FullyEncoded)
			: rewritten.error);
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void RuleBox(
		not_null<Ui::GenericBox*> box,
		QJsonObject config,
		int index) {
	box->setTitle(tr::lng_nagram_link_rule());

	const auto rules = config.value(u"rules"_q).toArray();
	const auto rule = index < rules.size() ? rules[index].toObject() : QJsonObject{
		{ u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces) },
		{ u"host"_q, QString() },
		{ u"replacementHost"_q, QString() },
		{ u"removeParameters"_q, QJsonArray() },
		{ u"enabled"_q, false },
	};
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_link_rule_about(), st::boxLabel));
	const auto host = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_link_host(), rule.value(u"host"_q).toString()));
	const auto replacement = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_link_replacement(), rule.value(u"replacementHost"_q).toString()));
	auto names = QStringList();
	for (const auto &name : rule.value(u"removeParameters"_q).toArray()) {
		names.push_back(name.toString());
	}
	const auto parameters = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::MultiLine,
		tr::lng_nagram_link_parameters(), names.join('\n')));
	host->setMaxLength(253);
	replacement->setMaxLength(253);
	parameters->setMaxLength(2200);
	const auto enabled = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_link_enabled(tr::now), rule.value(u"enabled"_q).toBool()));
	const auto updated = [=] {
		auto changed = rule;
		changed.insert(u"host"_q, host->getLastText().trimmed().toLower());
		changed.insert(u"replacementHost"_q, replacement->getLastText().trimmed().toLower());
		changed.insert(u"enabled"_q, enabled->checked());
		auto names = QJsonArray();
		for (const auto &part : parameters->getLastText().split('\n', Qt::SkipEmptyParts)) {
			names.push_back(part.trimmed());
		}
		changed.insert(u"removeParameters"_q, names);
		auto result = config;
		auto rules = config.value(u"rules"_q).toArray();
		if (index < rules.size()) {
			rules[index] = changed;
		} else {
			rules.push_back(changed);
		}
		result.insert(u"rules"_q, rules);
		return result;
	};
	box->addButton(tr::lng_settings_save(), [=] {
		if (Save(box, config, updated())) {
			box->closeBox();
		}
	});
	box->addButton(tr::lng_nagram_link_preview(), [=] {
		auto sample = updated();
		if (!Valid(sample)) {
			box->showToast(tr::lng_nagram_link_invalid(tr::now));
			return;
		}
		auto rules = sample.value(u"rules"_q).toArray();
		auto rule = rules[index].toObject();
		rule.insert(u"enabled"_q, true);
		sample.insert(u"rules"_q, QJsonArray{ rule });
		box->getDelegate()->show(Box(PreviewBox, sample), Ui::LayerOption::KeepOther);
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void SettingsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_link_rules());

	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_link_rules_about(), st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = [=] {
		rows->clear();
		const auto config = Current();
		if (!config) {
			rows->add(object_ptr<Ui::FlatLabel>(rows, tr::lng_nagram_link_invalid(), st::boxLabel));
			return;
		}
		const auto confirm = rows->add(object_ptr<Ui::Checkbox>(
			rows, tr::lng_nagram_link_confirm_all(tr::now), config->value(u"confirmAll"_q).toBool()));
		confirm->checkedChanges() | rpl::on_next([=](bool value) {
			auto updated = *config;
			updated.insert(u"confirmAll"_q, value);
			Save(box, *config, updated);
		}, confirm->lifetime());
		const auto rules = config->value(u"rules"_q).toArray();
		for (auto index = 0; index < rules.size(); ++index) {
			const auto rule = rules[index].toObject();
			const auto button = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(rule.value(u"host"_q).toString()
					+ (rule.value(u"enabled"_q).toBool() ? QString() : u" · "_q + tr::lng_nagram_config_off(tr::now))),
				st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				const auto menu = Ui::CreateChild<Ui::PopupMenu>(box);
				menu->addAction(tr::lng_nagram_link_rule(tr::now), [=] {
					box->getDelegate()->show(Box(RuleBox, *config, index), Ui::LayerOption::KeepOther);
				});
				for (const auto &delta : { -1, 1 }) {
					if (index + delta < 0 || index + delta >= rules.size()) {
						continue;
					}
					menu->addAction(delta < 0 ? tr::lng_link_move_up(tr::now) : tr::lng_link_move_down(tr::now), [=] {
						auto ordered = rules;
						const auto entry = ordered.takeAt(index);
						ordered.insert(index + delta, entry);
						auto updated = *config;
						updated.insert(u"rules"_q, ordered);
						Save(box, *config, updated);
					});
				}
				menu->addAction(tr::lng_box_delete(tr::now), [=] {
					auto changed = rules;
					changed.removeAt(index);
					auto updated = *config;
					updated.insert(u"rules"_q, changed);
					Save(box, *config, updated);
				});
				menu->popup(QCursor::pos());
			});
		}
	};
	ForDevice().changes() | rpl::on_next([=](std::string_view) {
		InvokeQueued(box, refresh);
	}, box->lifetime());
	box->addButton(tr::lng_nagram_link_add(), [=] {
		if (const auto config = Current()) {
			box->getDelegate()->show(Box(RuleBox, *config, int(config->value(u"rules"_q).toArray().size())), Ui::LayerOption::KeepOther);
		}
	});
	box->addButton(tr::lng_nagram_link_preview(), [=] {
		if (const auto config = Current()) {
			box->getDelegate()->show(Box(PreviewBox, *config), Ui::LayerOption::KeepOther);
		}
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	refresh();
}

} // namespace Nagram::Links
