#include "regram/nagram/filters/settings.h"
#include "regram/nagram/filters/model.h"

#include "base/unique_qptr.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_forum_topic.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/core/regex.h"

#include "settings/settings_builder.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram::Filters {
namespace {

struct Target {
	enum class Kind { Account, Global, Scope };
	Kind kind = Kind::Account;
	QString peer;
	QString topic;
};

struct Raw {
	QByteArray account;
	QByteArray global;
	QByteArray scopes;
};

QByteArray Bytes(const QJsonObject &value, const QJsonObject &defaults) {
	return (value == defaults)
		? QByteArray()
		: QJsonDocument(value).toJson(QJsonDocument::Compact);
}

QJsonObject Object(const QByteArray &raw, const QJsonObject &defaults) {
	return raw.isEmpty() ? defaults : QJsonDocument::fromJson(raw).object();
}

QString ValidationError(const QJsonObject &value) {
	return Validate(QJsonDocument(value).toJson(QJsonDocument::Compact))
		? QString() : tr::lng_nagram_filter_invalid(tr::now);
}

Raw ReadRaw(not_null<Main::Session*> session) {
	return {
		ForAccount(session).Get(kRules),
		ForDevice().Get(kGlobalRules),
		ForAccount(session).Get(kScopes),
	};
}

QJsonObject ReadFilters(not_null<Main::Session*> session) {
	return Object(ForAccount(session).Get(kRules), Defaults());
}

bool Fits(
		not_null<Ui::GenericBox*> box,
		const Raw &raw,
		const QString &peer,
		const QString &topic) {
	const auto resolved = Resolve(
		raw.account,
		raw.global,
		raw.scopes,
		peer,
		topic);
	if (resolved.error.isEmpty()) {
		return true;
	}
	box->showToast((resolved.count > kMaxRules)
		? tr::lng_nagram_filter_too_many(
			tr::now,
			lt_amount,
			QString::number(resolved.count),
			lt_limit,
			QString::number(kMaxRules))
		: tr::lng_nagram_filter_invalid(tr::now));
	return false;
}

void Write(
		not_null<Main::Session*> session,
		const Raw &before,
		const Raw &after) {
	if (after.account != before.account) {
		Expects(ForAccount(session).Set(kRules, after.account));
	}
	if (after.global != before.global) {
		Expects(ForDevice().Set(kGlobalRules, after.global));
	}
	if (after.scopes != before.scopes) {
		Expects(ForAccount(session).Set(kScopes, after.scopes));
	}
}

bool Save(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QJsonObject &expected,
		const QJsonObject &value) {
	const auto before = ReadRaw(session);
	auto raw = before;
	if (ReadFilters(session) != expected) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	}
	if (const auto error = ValidationError(value); !error.isEmpty()) {
		box->showToast(error);
		return false;
	}
	raw.account = Bytes(value, Defaults());
	if (!Fits(box, raw, QString(), u"0"_q)) {
		return false;
	}
	Write(session, before, raw);
	return true;
}

int ScopeIndex(
		const QJsonArray &scopes,
		const QString &peer,
		const QString &topic) {
	for (auto i = 0; i != scopes.size(); ++i) {
		const auto scope = scopes[i].toObject();
		if (scope.value(u"peer"_q) == peer
			&& scope.value(u"topic"_q) == topic) {
			return i;
		}
	}
	return -1;
}

QJsonObject ReadScope(
		not_null<Main::Session*> session,
		const QString &peer,
		const QString &topic) {
	const auto scopes = Object(ForAccount(session).Get(kScopes),
		ScopeDefaults()).value(u"scopes"_q).toArray();
	const auto index = ScopeIndex(scopes, peer, topic);
	return (index < 0) ? NewScope(peer, topic) : scopes[index].toObject();
}

bool SaveScope(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QJsonObject &expected,
		QJsonObject scope) {
	const auto before = ReadRaw(session);
	auto raw = before;
	if (!ValidateScopes(raw.scopes) || !Validate(raw.account)) {
		box->showToast(tr::lng_nagram_filter_scopes_invalid(tr::now));
		return false;
	}
	const auto peer = scope.value(u"peer"_q).toString();
	const auto topic = scope.value(u"topic"_q).toString();
	if (ReadScope(session, peer, topic) != expected) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	}
	auto disabled = QJsonArray();
	const auto inherited = InheritedRules(
		raw.account,
		raw.global,
		raw.scopes,
		peer,
		topic);
	for (const auto &entry : inherited) {
		const auto id = entry.rule.value(u"id"_q);
		if (scope.value(u"disabledRules"_q).toArray().contains(id)) {
			disabled.push_back(id);
		}
	}
	scope.insert(u"disabledRules"_q, disabled);
	auto settings = Object(raw.scopes, ScopeDefaults());
	auto scopes = settings.value(u"scopes"_q).toArray();
	const auto index = ScopeIndex(scopes, peer, topic);
	if (index >= 0) {
		scopes.removeAt(index);
	}
	if (!DefaultScope(scope)) {
		scopes.insert((index < 0) ? scopes.size() : index, scope);
		auto config = Object(raw.account, Defaults());
		auto excluded = config.value(u"excludedPeers"_q).toArray();
		for (auto i = excluded.size(); topic == u"0"_q && i != 0; --i) {
			if (excluded[i - 1] == peer) {
				excluded.removeAt(i - 1);
			}
		}
		config.insert(u"excludedPeers"_q, excluded);
		raw.account = Bytes(config, Defaults());
	}
	settings.insert(u"scopes"_q, scopes);
	raw.scopes = Bytes(settings, ScopeDefaults());
	if (!ValidateScopes(raw.scopes)) {
		box->showToast(tr::lng_nagram_filter_invalid(tr::now));
		return false;
	} else if (!Fits(box, raw, peer, topic)) {
		return false;
	}
	Write(session, before, raw);
	return true;
}

bool SaveOwn(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QJsonObject &expected,
		const QJsonObject &own) {
	const auto before = ReadRaw(session);
	auto raw = before;
	auto settings = Object(raw.scopes, ScopeDefaults());
	if (!ValidateScopes(raw.scopes)) {
		box->showToast(tr::lng_nagram_filter_scopes_invalid(tr::now));
		return false;
	} else if (settings.value(u"account"_q).toObject() != expected) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	}
	settings.insert(u"account"_q, own);
	raw.scopes = Bytes(settings, ScopeDefaults());
	if (!ValidateScopes(raw.scopes)) {
		box->showToast(tr::lng_nagram_filter_invalid(tr::now));
		return false;
	} else if (!Fits(box, raw, QString(), u"0"_q)) {
		return false;
	}
	Write(session, before, raw);
	return true;
}

std::optional<QJsonArray> ReadRules(
		not_null<Main::Session*> session,
		const Target &target) {
	const auto raw = ReadRaw(session);
	switch (target.kind) {
	case Target::Kind::Account:
		return Validate(raw.account)
			? Object(raw.account, Defaults()).value(u"rules"_q).toArray()
			: std::optional<QJsonArray>();
	case Target::Kind::Global:
		return ValidateGlobal(raw.global)
			? Object(raw.global, GlobalDefaults()).value(
				u"rules"_q).toArray()
			: std::optional<QJsonArray>();
	case Target::Kind::Scope:
		return ValidateScopes(raw.scopes)
			? ReadScope(session, target.peer, target.topic).value(
				u"rules"_q).toArray()
			: std::optional<QJsonArray>();
	}
	Unexpected("Target kind in Filters::ReadRules.");
}

bool SaveRules(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const Target &target,
		const QJsonArray &expected,
		const QJsonArray &rules) {
	if (ReadRules(session, target) != expected) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	} else if (target.kind == Target::Kind::Account) {
		const auto current = ReadFilters(session);
		auto updated = current;
		updated.insert(u"rules"_q, rules);
		return Save(box, session, current, updated);
	} else if (target.kind == Target::Kind::Scope) {
		const auto current = ReadScope(session, target.peer, target.topic);
		auto updated = current;
		updated.insert(u"rules"_q, rules);
		return SaveScope(box, session, current, updated);
	}
	const auto before = ReadRaw(session);
	auto raw = before;
	auto config = GlobalDefaults();
	config.insert(u"rules"_q, rules);
	raw.global = Bytes(config, GlobalDefaults());
	if (!ValidateGlobal(raw.global)) {
		box->showToast(tr::lng_nagram_filter_invalid(tr::now));
		return false;
	} else if (!Fits(box, raw, QString(), u"0"_q)) {
		return false;
	}
	Write(session, before, raw);
	return true;
}

void WatchChanges(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		Fn<void()> refresh) {
	rpl::merge(
		ForDevice().changes(),
		ForAccount(session).changes()
	) | rpl::on_next([=](std::string_view) {
		InvokeQueued(box, refresh);
	}, box->lifetime());
	refresh();
}

not_null<Ui::SettingsButton*> AddRow(
		not_null<Ui::VerticalLayout*> rows,
		QString text,
		Fn<void()> click = nullptr) {
	const auto row = rows->add(object_ptr<Ui::SettingsButton>(
		rows, rpl::single(std::move(text)), st::settingsButtonNoIcon));
	if (click) {
		row->setClickedCallback(std::move(click));
	}
	return row;
}

void PreviewBox(not_null<Ui::GenericBox*> box, QJsonObject config) {
	box->setTitle(tr::lng_nagram_filter_preview());

	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::MultiLine));
	input->setMaxLength(16384);
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_filter_preview_about(), st::boxLabel));
	result->setSelectable(true);
	const auto preview = [=] {
		if (const auto error = ValidationError(config); !error.isEmpty()) {
			result->setText(error);
			return;
		}
		auto active = config;
		active.insert(u"enabled"_q, true);
		const auto text = TextWithEntities{ input->getLastText() };
		const auto value = Apply(
			QJsonDocument(active).toJson(QJsonDocument::Compact),
			text, QString(), QString(), false, false, text.text);
		result->setText(!value.error.isEmpty() ? value.error
			: value.hidden ? tr::lng_nagram_filter_hidden(tr::now)
			: value.text.text);
	};
	input->changes() | rpl::on_next(preview, input->lifetime());
	box->addButton(tr::lng_nagram_filter_preview(), preview);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void RuleBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		Target target,
		QJsonArray rules,
		int index) {
	box->setTitle(tr::lng_nagram_filter_rule());

	const auto original = index < rules.size() ? rules[index].toObject() : QJsonObject{
		{ u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces) },
		{ u"title"_q, QString() },
		{ u"pattern"_q, QString() },
		{ u"enabled"_q, false },
		{ u"caseInsensitive"_q, false },
		{ u"reversed"_q, false },
		{ u"action"_q, u"mask"_q },
		{ u"replacement"_q, QString() },
	};
	const auto field = [&](rpl::producer<QString> label, QString key, int maximum) {
		box->addRow(object_ptr<Ui::FlatLabel>(box, std::move(label), st::boxLabel));
		const auto input = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
			rpl::single(QString()), original.value(key).toString()));
		input->setMaxLength(maximum);
		return input;
	};
	const auto title = field(tr::lng_nagram_filter_title(), u"title"_q, 128);
	const auto pattern = field(tr::lng_nagram_filter_pattern(), u"pattern"_q, 2048);
	const auto replacement = field(tr::lng_nagram_filter_replacement(), u"replacement"_q, 4096);
	const auto flags = std::array{
		std::pair(u"enabled"_q, tr::lng_nagram_filter_rule_enabled(tr::now)),
		std::pair(u"caseInsensitive"_q, tr::lng_nagram_filter_case(tr::now)),
		std::pair(u"reversed"_q, tr::lng_nagram_filter_reverse(tr::now)),
	};
	auto toggles = std::vector<Ui::Checkbox*>();
	for (const auto &[key, label] : flags) {
		toggles.push_back(box->addRow(object_ptr<Ui::Checkbox>(
			box, label, original.value(key).toBool())));
	}
	const auto actions = QStringList{ u"mask"_q, u"replace"_q, u"hide"_q };
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		actions.indexOf(original.value(u"action"_q).toString()));
	const auto labels = std::array{
		tr::lng_nagram_filter_mask(tr::now), tr::lng_nagram_filter_replace(tr::now),
		tr::lng_nagram_filter_hide(tr::now),
	};
	for (auto i = 0; i != labels.size(); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(box, group, i, labels[i]));
	}
	const auto collect = [=] {
		auto rule = original;
		rule.insert(u"title"_q, title->getLastText().trimmed());
		rule.insert(u"pattern"_q, pattern->getLastText());
		rule.insert(u"replacement"_q, replacement->getLastText());
		for (auto i = 0; i != flags.size(); ++i) {
			rule.insert(flags[i].first, toggles[i]->checked());
		}
		rule.insert(u"action"_q, actions[group->current()]);
		return rule;
	};
	const auto checked = [=](const QJsonObject &rule) {
		const auto problem = Regex::Check(
			rule.value(u"pattern"_q).toString(),
			rule.value(u"caseInsensitive"_q).toBool()
				? QRegularExpression::CaseInsensitiveOption
				: QRegularExpression::NoPatternOption);
		if (problem) {
			box->showToast(tr::lng_nagram_filter_pattern_error(
				tr::now,
				lt_index,
				QString::number(problem->position),
				lt_error,
				problem->text));
		}
		return !problem;
	};
	const auto save = [=](const QJsonArray &updated) {
		if (SaveRules(box, session, target, rules, updated)) {
			box->closeBox();
		}
	};
	box->addButton(tr::lng_settings_save(), [=] {
		const auto rule = collect();
		if (!checked(rule)) {
			return;
		}
		auto list = rules;
		if (index < list.size()) {
			list[index] = rule;
		} else {
			list.push_back(rule);
		}
		save(list);
	});
	box->addButton(tr::lng_nagram_filter_preview(), [=] {
		auto rule = collect();
		if (!checked(rule)) {
			return;
		}
		rule.insert(u"enabled"_q, true);
		auto config = Defaults();
		config.insert(u"rules"_q, QJsonArray{ rule });
		box->uiShow()->showBox(Box(PreviewBox, config));
	});
	if (index < rules.size()) {
		for (const auto &delta : { -1, 1 }) {
			if (index + delta < 0 || index + delta >= rules.size()) {
				continue;
			}
			box->addButton(delta < 0 ? tr::lng_link_move_up() : tr::lng_link_move_down(), [=] {
				auto list = rules;
				const auto rule = list.takeAt(index);
				list.insert(index + delta, rule);
				save(list);
			});
		}
		box->addButton(tr::lng_box_delete(), [=] {
			auto list = rules;
			list.removeAt(index);
			save(list);
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void AddRuleRows(
		not_null<Ui::VerticalLayout*> rows,
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const Target &target,
		const QJsonArray &rules) {
	for (auto i = 0; i < rules.size(); ++i) {
		const auto rule = rules[i].toObject();
		AddRow(rows, QString::number(i + 1) + u". "_q
			+ rule.value(u"title"_q).toString()
			+ (rule.value(u"enabled"_q).toBool()
				? QString()
				: u" · "_q + tr::lng_nagram_config_off(tr::now)), [=] {
			box->uiShow()->showBox(Box(RuleBox, session, target, rules, i));
		});
	}
	AddRow(rows, tr::lng_nagram_filter_add(tr::now), [=] {
		box->uiShow()->showBox(
			Box(RuleBox, session, target, rules, int(rules.size())));
	});
}

void AddTemplateRows(
		not_null<Ui::VerticalLayout*> rows,
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const Target &target,
		const QJsonArray &rules) {
	AddRow(rows, tr::lng_nagram_filter_export(tr::now), [=] {
		auto list = rules;
		for (auto i = 0; i < list.size(); ++i) {
			auto rule = list[i].toObject();
			rule.insert(u"enabled"_q, false);
			list[i] = rule;
		}
		const auto data = QJsonObject{ { u"version"_q, 1 }, { u"rules"_q, list } };
		QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(data).toJson()));
		box->showToast(tr::lng_nagram_filter_exported(tr::now));
	});
	AddRow(rows, tr::lng_nagram_filter_import(tr::now), [=] {
		const auto text = QApplication::clipboard()->text();
		if (text.size() > 128 * 1024) {
			box->showToast(tr::lng_nagram_filter_invalid(tr::now));
			return;
		}
		const auto data = QJsonDocument::fromJson(text.toUtf8()).object();
		if (data.size() != 2
			|| data.value(u"version"_q) != 1 || !data.value(u"rules"_q).isArray()) {
			box->showToast(tr::lng_nagram_filter_invalid(tr::now));
			return;
		}
		auto imported = data.value(u"rules"_q).toArray();
		auto validated = Defaults();
		validated.insert(u"rules"_q, imported);
		if (const auto error = ValidationError(validated); !error.isEmpty()) {
			box->showToast(error);
			return;
		}
		auto names = QStringList();
		auto list = rules;
		for (const auto &entry : imported) {
			auto rule = entry.toObject();
			rule.insert(u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces));
			rule.insert(u"enabled"_q, false);
			names.push_back(rule.value(u"title"_q).toString());
			list.push_back(rule);
		}
		box->uiShow()->showBox(Ui::MakeConfirmBox({
			.text = tr::lng_nagram_filter_import_about(tr::now) + u"\n\n"_q + names.join('\n'),
			.confirmed = crl::guard(box, [=](Fn<void()> close) {
				if (SaveRules(box, session, target, rules, list)) {
					close();
				}
			}),
		}));
	});
}

void PeerListBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		QJsonObject current,
		QString key) {
	box->setTitle(key == u"hiddenAuthors"_q
		? tr::lng_nagram_filter_hidden_authors()
		: tr::lng_nagram_filter_excluded_chats());
	box->addRow(object_ptr<Ui::FlatLabel>(box,
		tr::lng_nagram_filter_id_help(), st::boxLabel));
	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_filter_add_id()));
	input->setMaxLength(20);
	box->addButton(tr::lng_nagram_filter_add(), [=] {
		const auto id = input->getLastText().trimmed();
		auto list = current.value(key).toArray();
		if (!list.contains(id)) {
			list.push_back(id);
		}
		auto updated = current;
		updated.insert(key, list);
		if (Save(box, session, current, updated)) {
			box->closeBox();
		}
	});
	const auto list = current.value(key).toArray();
	for (auto index = 0; index != list.size(); ++index) {
		const auto id = list[index].toString();
		const auto row = box->addRow(object_ptr<Ui::SettingsButton>(
			box, rpl::single(id), st::settingsButtonNoIcon));
		row->setClickedCallback([=] {
			auto updated = current;
			auto values = list;
			values.removeAt(index);
			updated.insert(key, values);
			if (Save(box, session, current, updated)) {
				box->closeBox();
			}
		});
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void InvalidBox(not_null<Ui::GenericBox*> box, rpl::producer<QString> text) {
	box->addRow(object_ptr<Ui::FlatLabel>(box, std::move(text), st::boxLabel));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FiltersBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_filters());

	if (!Validate(ForAccount(session).Get(kRules))) {
		InvalidBox(box, tr::lng_nagram_filter_invalid());
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_filters_about(), st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = std::make_shared<Fn<void()>>();
	*refresh = [=] {
		rows->clear();
		const auto raw = ForAccount(session).Get(kRules);
		if (!Validate(raw)) {
			return;
		}
		const auto current = Object(raw, Defaults());
		for (const auto &[key, label] : std::array{
			std::pair(u"enabled"_q, tr::lng_nagram_filter_enabled(tr::now)),
			std::pair(u"filterOutgoing"_q, tr::lng_nagram_filter_outgoing(tr::now)),
			std::pair(u"hideBlocked"_q, tr::lng_nagram_filter_blocked(tr::now)),
			std::pair(u"stripZalgo"_q, tr::lng_nagram_filter_zalgo(tr::now)),
		}) {
			const auto row = AddRow(rows, label);
			row->toggleOn(rpl::single(current.value(key).toBool()));
			row->toggledChanges() | rpl::on_next([=](bool value) {
				auto updated = current;
				updated.insert(key, value);
				if (!Save(box, session, current, updated)) {
					InvokeQueued(box, *refresh);
				}
			}, row->lifetime());
		}
		for (const auto &[key, label] : std::array{
			std::pair(u"hiddenAuthors"_q,
				tr::lng_nagram_filter_hidden_authors(tr::now)),
			std::pair(u"excludedPeers"_q,
				tr::lng_nagram_filter_excluded_chats(tr::now)),
		}) {
			AddRow(rows, label, [=] {
				box->uiShow()->showBox(Box(PeerListBox, session, current, key));
			});
		}
		const auto rules = current.value(u"rules"_q).toArray();
		AddRuleRows(rows, box, session, {}, rules);
		AddRow(rows, tr::lng_nagram_filter_preview(tr::now), [=] {
			box->uiShow()->showBox(Box(PreviewBox, current));
		});
		AddTemplateRows(rows, box, session, {}, rules);
	};
	box->lifetime().add([=] { *refresh = nullptr; });
	WatchChanges(box, session, [=] { (*refresh)(); });
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

QString LayerName(Layer layer) {
	switch (layer) {
	case Layer::Global: return tr::lng_nagram_filter_scope_source_global(tr::now);
	case Layer::Account: return tr::lng_nagram_filter_scope_source_account(tr::now);
	case Layer::Chat: return tr::lng_nagram_filter_scope_source_chat(tr::now);
	}
	Unexpected("Layer in Filters::LayerName.");
}

QString ScopeName(
		not_null<Main::Session*> session,
		const QString &peer,
		const QString &topic) {
	const auto loaded = session->data().peerLoaded(
		DeserializePeerId(peer.toULongLong()));
	auto result = loaded ? loaded->name() : peer;
	if (topic != u"0"_q) {
		const auto thread = loaded
			? loaded->forumTopicFor(MsgId(topic.toLongLong()))
			: nullptr;
		result += u" › "_q + (thread ? thread->title() : topic);
	}
	return result;
}

QString ScopeSummary(const QJsonObject &scope) {
	const auto state = scope.value(u"enabled"_q).toString();
	auto parts = QStringList{ (state == u"on"_q)
		? tr::lng_nagram_filter_scope_on(tr::now)
		: (state == u"off"_q)
		? tr::lng_nagram_filter_scope_off(tr::now)
		: tr::lng_nagram_filter_scope_inherit(tr::now) };
	if (const auto count = scope.value(u"disabledRules"_q).toArray().size()) {
		parts.push_back(tr::lng_nagram_filter_scope_summary_disabled(
			tr::now, lt_amount, QString::number(count)));
	}
	if (const auto count = scope.value(u"rules"_q).toArray().size()) {
		parts.push_back(tr::lng_nagram_filter_scope_summary_own(
			tr::now, lt_amount, QString::number(count)));
	}
	return parts.join(u" · "_q);
}

void ConfirmRestore(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QJsonObject &scope) {
	const auto peer = scope.value(u"peer"_q).toString();
	const auto topic = scope.value(u"topic"_q).toString();
	box->uiShow()->showBox(Ui::MakeConfirmBox({
		.text = tr::lng_nagram_filter_scope_restore_sure(),
		.confirmed = crl::guard(box, [=](Fn<void()> close) {
			SaveScope(box, session, scope, NewScope(peer, topic));
			close();
		}),
	}));
}

void ScopeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		QString peer,
		QString topic) {
	box->setTitle(rpl::single(ScopeName(session, peer, topic)));

	const auto read = ReadRaw(session);
	if (!ValidateScopes(read.scopes) || !Validate(read.account)) {
		InvalidBox(box, tr::lng_nagram_filter_scopes_invalid());
		return;
	}
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = std::make_shared<Fn<void()>>();
	*refresh = [=] {
		rows->clear();
		const auto raw = ReadRaw(session);
		if (!ValidateScopes(raw.scopes) || !Validate(raw.account)) {
			return;
		}
		const auto scope = ReadScope(session, peer, topic);
		const auto failed = [=] { InvokeQueued(box, *refresh); };
		rows->add(object_ptr<Ui::FlatLabel>(
			rows, tr::lng_nagram_filter_scope_state(), st::boxLabel));
		const auto inherited = InheritedEnabled(
			raw.account, raw.scopes, peer, topic);
		const auto states = QStringList{ u"inherit"_q, u"on"_q, u"off"_q };
		const auto group = std::make_shared<Ui::RadiobuttonGroup>(
			states.indexOf(scope.value(u"enabled"_q).toString()));
		const auto labels = std::array{
			tr::lng_nagram_filter_scope_inherit_value(
				tr::now,
				lt_value,
				inherited.enabled
					? tr::lng_nagram_filter_scope_on(tr::now)
					: tr::lng_nagram_filter_scope_off(tr::now),
				lt_source,
				LayerName(inherited.layer)),
			tr::lng_nagram_filter_scope_on(tr::now),
			tr::lng_nagram_filter_scope_off(tr::now),
		};
		for (auto i = 0; i != labels.size(); ++i) {
			rows->add(object_ptr<Ui::Radiobutton>(rows, group, i, labels[i]));
		}
		group->setChangedCallback([=](int value) {
			auto updated = scope;
			updated.insert(u"enabled"_q, states[value]);
			if (!SaveScope(box, session, scope, updated)) {
				failed();
			}
		});
		const auto upper = InheritedRules(
			raw.account, raw.global, raw.scopes, peer, topic);
		if (!upper.empty()) {
			rows->add(object_ptr<Ui::FlatLabel>(
				rows, tr::lng_nagram_filter_scope_inherited(), st::boxLabel));
		}
		const auto disabled = scope.value(u"disabledRules"_q).toArray();
		for (const auto &entry : upper) {
			const auto id = entry.rule.value(u"id"_q);
			const auto row = AddRow(rows,
				entry.rule.value(u"title"_q).toString()
					+ u" · "_q + LayerName(entry.layer));
			row->toggleOn(rpl::single(!disabled.contains(id)));
			row->toggledChanges() | rpl::on_next([=](bool value) {
				auto list = disabled;
				for (auto i = list.size(); i != 0; --i) {
					if (list[i - 1] == id) {
						list.removeAt(i - 1);
					}
				}
				if (!value) {
					list.push_back(id);
				}
				auto updated = scope;
				updated.insert(u"disabledRules"_q, list);
				if (!SaveScope(box, session, scope, updated)) {
					failed();
				}
			}, row->lifetime());
		}
		rows->add(object_ptr<Ui::FlatLabel>(
			rows, tr::lng_nagram_filter_scope_own(), st::boxLabel));
		AddRuleRows(rows, box, session,
			{ Target::Kind::Scope, peer, topic },
			scope.value(u"rules"_q).toArray());
		AddRow(rows, tr::lng_nagram_filter_preview(tr::now), [=] {
			const auto resolved = Resolve(
				raw.account, raw.global, raw.scopes, peer, topic);
			auto config = Object(resolved.config, Defaults());
			config.insert(u"enabled"_q, true);
			if (Fits(box, raw, peer, topic)) {
				box->uiShow()->showBox(Box(PreviewBox, config));
			}
		});
		if (!DefaultScope(scope)) {
			AddRow(rows, tr::lng_nagram_filter_scope_restore(tr::now), [=] {
				ConfirmRestore(box, session, scope);
			});
		}
	};
	box->lifetime().add([=] { *refresh = nullptr; });
	WatchChanges(box, session, [=] { (*refresh)(); });
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void GlobalBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_filter_global());

	if (!ValidateGlobal(ForDevice().Get(kGlobalRules))) {
		InvalidBox(box, tr::lng_nagram_filter_invalid());
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_filter_global_about(), st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto target = Target{ Target::Kind::Global };
	WatchChanges(box, session, [=] {
		rows->clear();
		const auto rules = ReadRules(session, target);
		if (!rules) {
			return;
		}
		AddRuleRows(rows, box, session, target, *rules);
		AddRow(rows, tr::lng_nagram_filter_preview(tr::now), [=] {
			auto config = Defaults();
			config.insert(u"rules"_q, *rules);
			box->uiShow()->showBox(Box(PreviewBox, config));
		});
		AddTemplateRows(rows, box, session, target, *rules);
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void ScopesBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_filter_scopes());

	if (!ValidateScopes(ForAccount(session).Get(kScopes))) {
		InvalidBox(box, tr::lng_nagram_filter_scopes_invalid());
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_filter_scopes_about(), st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = std::make_shared<Fn<void()>>();
	*refresh = [=] {
		rows->clear();
		const auto raw = ReadRaw(session);
		if (!ValidateScopes(raw.scopes)) {
			return;
		}
		const auto settings = Object(raw.scopes, ScopeDefaults());
		const auto own = settings.value(u"account"_q).toObject();
		const auto failed = [=] { InvokeQueued(box, *refresh); };
		const auto inherit = own.value(u"inheritGlobal"_q).toBool();
		const auto use = AddRow(rows, tr::lng_nagram_filter_use_global(tr::now));
		use->toggleOn(rpl::single(inherit));
		use->toggledChanges() | rpl::on_next([=](bool value) {
			auto updated = own;
			updated.insert(u"inheritGlobal"_q, value);
			if (!SaveOwn(box, session, own, updated)) {
				failed();
			}
		}, use->lifetime());
		const auto disabled = own.value(u"disabledRules"_q).toArray();
		const auto global = inherit
			? ReadRules(session, { Target::Kind::Global })
			: std::optional<QJsonArray>();
		for (const auto &entry : global.value_or(QJsonArray())) {
			const auto id = entry.toObject().value(u"id"_q);
			const auto row = AddRow(rows,
				entry.toObject().value(u"title"_q).toString());
			row->toggleOn(rpl::single(!disabled.contains(id)));
			row->toggledChanges() | rpl::on_next([=](bool value) {
				auto list = disabled;
				for (auto i = list.size(); i != 0; --i) {
					if (list[i - 1] == id) {
						list.removeAt(i - 1);
					}
				}
				if (!value) {
					list.push_back(id);
				}
				auto updated = own;
				updated.insert(u"disabledRules"_q, list);
				if (!SaveOwn(box, session, own, updated)) {
					failed();
				}
			}, row->lifetime());
		}
		const auto scopes = settings.value(u"scopes"_q).toArray();
		if (scopes.isEmpty()) {
			rows->add(object_ptr<Ui::FlatLabel>(
				rows, tr::lng_nagram_filter_scopes_empty(), st::boxLabel));
		}
		for (const auto &entry : scopes) {
			const auto scope = entry.toObject();
			const auto peer = scope.value(u"peer"_q).toString();
			const auto topic = scope.value(u"topic"_q).toString();
			const auto row = AddRow(rows, ScopeName(session, peer, topic)
				+ u" · "_q + ScopeSummary(scope));
			row->setClickedCallback([=] {
				const auto menu = Ui::CreateChild<Ui::PopupMenu>(box);
				menu->addAction((topic == u"0"_q)
					? tr::lng_nagram_filter_scope_chat(tr::now)
					: tr::lng_nagram_filter_scope_topic(tr::now), [=] {
					box->uiShow()->showBox(Box(ScopeBox, session, peer, topic));
				});
				menu->addAction(tr::lng_nagram_filter_scope_restore(tr::now), [=] {
					ConfirmRestore(box, session, scope);
				});
				menu->popup(QCursor::pos());
			});
		}
	};
	box->lifetime().add([=] { *refresh = nullptr; });
	WatchChanges(box, session, [=] { (*refresh)(); });
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

void SettingsBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	FiltersBox(box, session);
}

void GlobalSettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	GlobalBox(box, session);
}

void ScopesSettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	ScopesBox(box, session);
}

void ScopeSettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		QString peer,
		QString topic) {
	ScopeBox(box, session, std::move(peer), std::move(topic));
}

} // namespace Nagram::Filters
