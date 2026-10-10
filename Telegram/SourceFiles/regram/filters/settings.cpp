#include "regram/filters/settings.h"

#include "boxes/peer_list_controllers.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_peer.h"
#include "data/data_thread.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/filters/runtime.h"
#include "regram/filters/scopes.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QUuid>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Regram::Filters {
namespace {

constexpr auto kMaximumImportBytes = 8 * 1024 * 1024;

struct ViewState {
	StoredRules current;
	Fn<void()> refresh;
};

class ScopePicker final : public ChooseRecipientBoxController {
public:
	ScopePicker(
		not_null<Main::Session*> session,
		FnMut<void(not_null<Data::Thread*>)> selected);

protected:
	void prepareViewHook() override;

};

ScopePicker::ScopePicker(
		not_null<Main::Session*> session,
		FnMut<void(not_null<Data::Thread*>)> selected)
: ChooseRecipientBoxController(session, std::move(selected), [](const auto thread) {
	const auto peer = thread->peer();
	return peer->isUser() || peer->isChat() || peer->isChannel();
}) {
}

void ScopePicker::prepareViewHook() {
	delegate()->peerListSetTitle(tr::lng_regram_filter_choose_chats());
}

void BindSession(
		not_null<Ui::BoxContent*> box,
		not_null<Main::Session*> session) {
	const auto weak = QPointer<Ui::BoxContent>(box);
	session->lifetime().add([weak] {
		if (weak) {
			weak->closeBox();
		}
	});
}

void AddText(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(object_ptr<Ui::FlatLabel>(box, text, st::boxLabel));
}

void EditorBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		StoredRules before,
		size_t index) {
	BindSession(box, session);
	box->setTitle(tr::lng_nagram_filter_rules());
	const auto existing = (index < before.rules.size());
	const auto rule = existing ? before.rules[index] : Rule();
	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_filter_pattern(),
		QString::fromStdU16String(rule.pattern)));
	input->setMaxLength(kMaxPatternUnits);
	const auto enabled = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_filter_enabled(tr::now), rule.enabled,
		st::defaultBoxCheckbox));
	const auto regex = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_regram_filter_regex(tr::now),
		rule.regex,
		st::defaultBoxCheckbox));
	const auto all = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_regram_filter_all_chats(tr::now),
		rule.scopes.empty() && rule.unresolvedScopes.empty(),
		st::defaultBoxCheckbox));
	const auto initialScopes = FormatScopes(rule.scopes);
	const auto scopes = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::MultiLine,
		tr::lng_regram_filter_scope_hint(),
		initialScopes));
	scopes->setMaxLength(kMaxScopes * 64);
	const auto choose = box->addRow(object_ptr<Ui::SettingsButton>(
		box,
		tr::lng_regram_filter_choose_chats(),
		st::settingsButtonNoIcon));
	choose->setClickedCallback([=] {
		const auto picker = std::make_shared<base::weak_qptr<Ui::BoxContent>>();
		auto controller = std::make_unique<ScopePicker>(session, crl::guard(box, [=](
				not_null<Data::Thread*> thread) {
			auto parsed = ParseScopes(scopes->getLastText());
			if (!parsed.error.isEmpty()) {
				scopes->showError();
				box->showToast(tr::lng_regram_filter_scope_hint(tr::now));
				return;
			}
			const auto peer = thread->peer();
			const auto address = PeerAddress{
				.kind = peer->isUser() ? PeerKind::User
					: peer->isChat() ? PeerKind::Group : PeerKind::Channel,
				.id = peer->id.value & PeerId::kChatTypeMask,
				.thread = peer->isForum() ? uint64(thread->topicRootId().bare) : 0,
			};
			if (!ranges::contains(parsed.scopes, address)) {
				if (parsed.scopes.size() == kMaxScopes) {
					box->showToast(tr::lng_regram_filter_scope_limit(tr::now));
					return;
				}
				parsed.scopes.push_back(address);
			}
			scopes->setTextWithTags({ FormatScopes(parsed.scopes) });
			all->setChecked(false);
			if (*picker) {
				(*picker)->closeBox();
			}
		}));
		auto owned = Box<PeerListBox>(std::move(controller), [=](const auto picker) {
			BindSession(picker, session);
			picker->addButton(tr::lng_cancel(), [=] { picker->closeBox(); });
		});
		*picker = owned.data();
		box->uiShow()->showBox(std::move(owned));
	});
	all->checkedValue(
	) | rpl::on_next([=](bool checked) {
		scopes->setDisabled(checked);
	}, scopes->lifetime());
	if (!rule.scopes.empty() || !rule.unresolvedScopes.empty()) {
		AddText(box, tr::lng_regram_filter_scopes(tr::now,
			lt_count, int(rule.scopes.size() + rule.unresolvedScopes.size())));
	}
	for (const auto &problem : before.problems) {
		if (problem.ruleId == rule.id) {
			AddText(box, QString::fromStdString(problem.reason));
		}
	}
	const auto save = crl::guard(session, [=] {
		auto rules = before.rules;
		auto changed = rule;
		changed.pattern = input->getLastText().toStdU16String();
		changed.regex = regex->checked();
		changed.enabled = enabled->checked();
		if (all->checked()) {
			changed.scopes.clear();
			changed.unresolvedScopes.clear();
		} else {
			const auto text = scopes->getLastText();
			const auto parsed = ParseScopes(text);
			if (!parsed.error.isEmpty()
				|| (parsed.scopes.empty() && rule.unresolvedScopes.empty())) {
				scopes->showError();
				box->showToast(tr::lng_regram_filter_scope_hint(tr::now));
				return;
			}
			if (text != initialScopes) {
				changed.scopes = parsed.scopes;
				changed.unresolvedScopes.clear();
			}
		}
		if (existing) {
			rules[index] = std::move(changed);
		} else {
			changed.id = QUuid::createUuid().toString(
				QUuid::WithoutBraces).toStdString();
			rules.push_back(std::move(changed));
		}
		if (const auto error = Write(session, before.raw, rules)
			; !error.isEmpty()) {
			input->showError();
			box->showToast(error);
		} else {
			box->closeBox();
		}
	});
	box->setFocusCallback([=] { input->setFocusFast(); });
	box->addButton(tr::lng_settings_save(), save);
	if (existing) {
		box->addButton(tr::lng_box_delete(), [=] {
			auto rules = before.rules;
			rules.erase(rules.begin() + index);
			if (const auto error = Write(session, before.raw, rules)
				; !error.isEmpty()) {
				box->showToast(error);
			} else {
				box->closeBox();
			}
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void PreviewImport(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QByteArray &bytes) {
	const auto before = Read(session);
	const auto imported = ImportRules(bytes);
	if (!before.error.isEmpty() || !imported.error.isEmpty()) {
		box->showToast(before.error.isEmpty()
			? imported.error : before.error);
		return;
	}
	const auto merged = MergeRules(before.rules, imported.rules);
	if (!merged.error.isEmpty()) {
		box->showToast(merged.error);
		return;
	}
	box->uiShow()->showBox(Ui::MakeConfirmBox({
		.text = tr::lng_regram_filter_import_preview(tr::now,
			lt_count, merged.added,
			lt_amount, QString::number(merged.duplicates),
			lt_limit, QString::number(imported.problems.size())),
		.confirmed = crl::guard(session, crl::guard(box, [=](Fn<void()> close) {
			if (const auto error = Write(session, before.raw, merged.rules)
				; !error.isEmpty()) {
				box->showToast(error);
			} else {
				close();
			}
		})),
	}));
}

void ImportFile(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		tr::lng_nagram_config_import(tr::now),
		tr::lng_nagram_config_file_filter(tr::now),
		crl::guard(session, crl::guard(box, [=](FileDialog::OpenResult &&result) {
			if (!result.paths.isEmpty()) {
				auto file = QFile(result.paths.front());
				if (!file.open(QIODevice::ReadOnly)) {
					box->showToast(tr::lng_nagram_config_read_error(tr::now));
					return;
				}
				PreviewImport(box, session, file.read(kMaximumImportBytes + 1));
			} else if (!result.remoteContent.isEmpty()) {
				PreviewImport(box, session, result.remoteContent);
			}
		})));
}

void ExportFile(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	const auto current = Read(session);
	if (!current.error.isEmpty()) {
		box->showToast(current.error);
		return;
	}
	const auto bytes = ExportRules(current.rules);
	FileDialog::GetWritePath(
		Core::App().getFileDialogParent(),
		tr::lng_nagram_config_export(tr::now),
		tr::lng_nagram_config_file_filter(tr::now),
		u"regram-filters.json"_q,
		crl::guard(session, crl::guard(box, [=](QString &&path) {
			if (path.isEmpty()) {
				return;
			}
			auto file = QSaveFile(path);
			if (!file.open(QIODevice::WriteOnly)
				|| file.write(bytes) != bytes.size() || !file.commit()) {
				box->showToast(tr::lng_nagram_config_write_error(tr::now));
			} else {
				box->showToast(tr::lng_nagram_config_exported(tr::now));
			}
		})));
}

} // namespace

void SettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	BindSession(box, session);
	box->setTitle(tr::lng_regram_ios_filters());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_regram_ios_filters_about(), st::boxLabel));
	const auto notifications = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_regram_filter_mute_notifications(tr::now),
		Nagram::ForAccount(session).Get(kSuppressNotifications),
		st::defaultBoxCheckbox));
	notifications->checkedChanges(
	) | rpl::on_next([=](bool enabled) {
		Expects(Nagram::ForAccount(session).Set(kSuppressNotifications, enabled));
	}, notifications->lifetime());
	Nagram::ForAccount(session).Value(kSuppressNotifications)
	| rpl::on_next([=](bool enabled) {
		notifications->setChecked(enabled, Ui::Checkbox::NotifyAboutChange::DontNotify);
	},
		notifications->lifetime());
	const auto state = box->lifetime().make_state<ViewState>();
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	state->refresh = [=] {
		rows->clear();
		state->current = Read(session);
		if (!state->current.error.isEmpty()) {
			rows->add(object_ptr<Ui::FlatLabel>(
				rows, state->current.error, st::boxLabel));
			return;
		}
		for (auto index = size_t(); index != state->current.rules.size(); ++index) {
			const auto &rule = state->current.rules[index];
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows,
				rpl::single(QString::fromStdU16String(rule.pattern)
					+ (rule.enabled ? QString()
						: u" · "_q + tr::lng_nagram_config_off(tr::now))),
				st::settingsButtonNoIcon));
			row->setClickedCallback([=] {
				box->uiShow()->showBox(Box(EditorBox, session, state->current, index));
			});

		}
		const auto add = rows->add(object_ptr<Ui::SettingsButton>(
			rows, tr::lng_nagram_filter_add(), st::settingsButtonNoIcon));
		add->setClickedCallback([=] {
			box->uiShow()->showBox(Box(EditorBox, session,
				state->current, state->current.rules.size()));
		});
	};
	state->refresh();
	Nagram::ForAccount(session).changes(
	) | rpl::filter([](std::string_view key) {
		return key == kPortableRules.key;
	}) | rpl::on_next([=] {
		crl::on_main(box, [=] { state->refresh(); });
	}, box->lifetime());
	box->addButton(tr::lng_nagram_config_import(), [=] { ImportFile(box, session); });
	box->addButton(tr::lng_nagram_config_export(), [=] { ExportFile(box, session); });
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Regram::Filters
