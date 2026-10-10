#include "regram/nagram/settings/services.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/nagram/settings/home.h"

#include "core/application.h"
#include "core/file_utilities.h"
#include "core/core_settings.h"
#include "boxes/translate_box.h"
#include "lang/lang_keys.h"
#include "regram/nagram/services/auto_translate.h"
#include "regram/nagram/services/send_translation.h"
#include "regram/nagram/services/send_translation_model.h"
#include "main/main_session.h"
#include "regram/nagram/services/context_model.h"
#include "regram/nagram/services/credentials.h"
#include "regram/nagram/services/presets.h"
#include "regram/nagram/services/request.h"
#include "regram/nagram/services/summary_model.h"
#include "regram/nagram/services/system_ai.h"
#include "platform/platform_translate_provider.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/vertical_list.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>

#include <algorithm>

#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class ServicesSection final : public Section<ServicesSection> {
public:
	ServicesSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_services();
	}

	static const SectionBuildMethod kBuild;
};

bool Current(not_null<Ui::GenericBox*> box, const QJsonObject &expected) {
	if (Services() == expected) {
		return true;
	}
	box->showToast(tr::lng_nagram_config_changed_error(tr::now));
	return false;
}

QString DefaultTranslationId() {
	switch (AyuSettings::getInstance().translationProvider()) {
	case TranslationProvider::Telegram: return u":telegram"_q;
	case TranslationProvider::Google: return u":google"_q;
	case TranslationProvider::Yandex: return u":yandex"_q;
	case TranslationProvider::Native: return u":system"_q;
	}
	Unexpected("Translation provider");
}

QString DefaultTranslationName() {
	const auto id = DefaultTranslationId();
	return (id == u":google"_q) ? u"Google"_q
		: (id == u":yandex"_q) ? u"Yandex"_q
		: (id == u":system"_q) ? tr::lng_nagram_service_system(tr::now)
		: u"Telegram"_q;
}

QString TranslationSelectionName(const std::optional<QJsonObject> &config) {
	if (!config) {
		return tr::lng_nagram_service_invalid(tr::now);
	}
	const auto id = config->value(u"translation"_q).toString();
	if (id.isEmpty()) {
		return DefaultTranslationName();
	} else if (id == u"telegram"_q) {
		return u"Telegram"_q;
	} else if (id == u"system"_q) {
		return Platform::IsTranslateProviderAvailable()
			? tr::lng_nagram_service_system(tr::now)
			: tr::lng_nagram_system_translation_unavailable(tr::now);
	}
	const auto service = FindService(*config, id);
	return service ? service->name : tr::lng_nagram_service_invalid(tr::now);
}

void TranslationSourceBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_service_translation());
	const auto current = Services();
	if (!current) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_service_invalid(), st::boxLabel));
		return;
	}
	auto ids = QStringList{ u":telegram"_q, u":google"_q, u":yandex"_q, u":system"_q };
	auto titles = QStringList{
		u"Telegram"_q,
		u"Google"_q,
		u"Yandex"_q,
		tr::lng_nagram_service_system(tr::now),
	};
	for (const auto &value : current->value(u"instances"_q).toArray()) {
		const auto service = ParseService(value.toObject());
		if (service && service->kind == ServiceKind::Translation) {
			ids.push_back(service->id);
			titles.push_back(service->name);
		}
	}
	const auto id = current->value(u"translation"_q).toString();
	const auto selectedId = id.isEmpty() ? DefaultTranslationId()
		: (id == u"telegram"_q || id == u"system"_q) ? u":"_q + id : id;
	const auto selected = std::max<qsizetype>(0, ids.indexOf(selectedId));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	for (auto index = 0; index != ids.size(); ++index) {
		const auto row = box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, index, titles[index], st::settingsSendType),
			st::settingsSendTypePadding);
		if (index == 3 && !Platform::IsTranslateProviderAvailable()) {
			row->setDisabled(true);
		}
	}
	if (!Platform::IsTranslateProviderAvailable()) {
		box->addRow(object_ptr<Ui::FlatLabel>(box,
			tr::lng_nagram_system_translation_unavailable(), st::boxLabel));
	}
	group->setChangedCallback([=](int value) {
		if (!Current(box, *current)) {
			return;
		}
		auto updated = *current;
		const auto builtin = (value < 4);
		updated.insert(u"translation"_q, builtin ? QString() : ids[value]);
		if (!SetServices(updated)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		if (builtin) {
			const auto providers = std::array{
				TranslationProvider::Telegram,
				TranslationProvider::Google,
				TranslationProvider::Yandex,
				TranslationProvider::Native,
			};
			AyuSettings::getInstance().setTranslationProvider(providers[value]);
		}
		Core::App().saveSettingsDelayed();
		box->closeBox();
	});
}

QString SummarySelectionName(const std::optional<QJsonObject> &config) {
	if (!config) {
		return tr::lng_nagram_service_invalid(tr::now);
	}
	const auto id = config->value(u"summary"_q).toString();
	if (id.isEmpty()) {
		return tr::lng_nagram_summary_off(tr::now);
	}
	const auto service = FindService(*config, id);
	return service ? service->name : tr::lng_nagram_service_invalid(tr::now);
}

void SummarySourceBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_summary_service());
	const auto current = Services();
	if (!current) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_service_invalid(), st::boxLabel));
		return;
	}
	auto ids = QStringList{ QString() };
	auto titles = QStringList{ tr::lng_nagram_summary_off(tr::now) };
	for (const auto &value : current->value(u"instances"_q).toArray()) {
		const auto service = ParseService(value.toObject());
		if (service
			&& service->kind == ServiceKind::Translation
			&& LlmProtocol(service->protocol)) {
			ids.push_back(service->id);
			titles.push_back(service->name);
		}
	}
	const auto selected = std::max<qsizetype>(0, ids.indexOf(
		current->value(u"summary"_q).toString()));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	for (auto index = 0; index != ids.size(); ++index) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, index, titles[index], st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (!Current(box, *current)) {
			return;
		}
		auto updated = *current;
		updated.insert(u"summary"_q, ids[value]);
		if (!SetServices(updated)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		Core::App().saveSettingsDelayed();
		box->closeBox();
	});
}

QString TranscriptionSelectionName(const std::optional<QJsonObject> &config) {
	if (!config) {
		return tr::lng_nagram_service_invalid(tr::now);
	}
	const auto id = config->value(u"transcription"_q).toString();
	if (id.isEmpty() || id == u"telegram"_q) {
		return u"Telegram"_q;
	}
	const auto service = FindService(*config, id);
	return service ? service->name : tr::lng_nagram_service_invalid(tr::now);
}

void TranscriptionSourceBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_service_transcription());
	const auto current = Services();
	if (!current) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_service_invalid(), st::boxLabel));
		return;
	}
	auto ids = QStringList{ QString() };
	auto titles = QStringList{ u"Telegram"_q };
	for (const auto &value : current->value(u"instances"_q).toArray()) {
		const auto service = ParseService(value.toObject());
		if (service && service->kind == ServiceKind::Transcription) {
			ids.push_back(service->id);
			titles.push_back(service->name);
		}
	}
	const auto selectedId = current->value(u"transcription"_q).toString();
	const auto selected = std::max<qsizetype>(0, ids.indexOf(selectedId));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(selected);
	for (auto index = 0; index != ids.size(); ++index) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, index, titles[index], st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (!Current(box, *current)) {
			return;
		}
		auto updated = *current;
		updated.insert(u"transcription"_q, ids[value]);
		if (!SetServices(updated)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		Core::App().saveSettingsDelayed();
		box->closeBox();
	});
}

bool CredentialUsed(const QJsonObject &config, const QString &account) {
	for (const auto &value : config.value(u"instances"_q).toArray()) {
		const auto service = ParseService(value.toObject());
		if (service && CredentialAccount(*service) == account) {
			return true;
		}
	}
	return false;
}

CredentialError RemoveServiceCredential(const ServiceDefinition &service) {
	const auto account = CredentialAccount(service);
	if (!service.useKey && ReadCredential(account).error == CredentialError::Unavailable) {
		return CredentialError::None;
	}
	return DeleteCredential(account);
}

void ServiceTestBox(
		not_null<Ui::GenericBox*> box,
		ServiceDefinition service,
		Fn<void(QString)> chooseModel) {
	box->setTitle(tr::lng_nagram_service_test());

	box->addRow(object_ptr<Ui::FlatLabel>(box, rpl::single(
		service.name + u"\n"_q + ServiceEndpoint(service).toDisplayString()
		+ u"\n"_q + tr::lng_nagram_service_test_about(tr::now)), st::boxLabel));
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box, rpl::single(QString()), st::boxLabel));
	result->setSelectable(true);
	struct State {
		ServiceRequest request;
		int generation = 0;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto request = &state->request;
	const auto generation = &state->generation;
	const auto modelRows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto stop = [=] {
		++*generation;
		request->cancel();
		modelRows->clear();
	};
	const auto status = [=](ServiceResult response) {
		result->setText(ServiceErrorText(response.error, response.status));
	};
	if (LlmProtocol(service.protocol)) {
		box->addButton(tr::lng_nagram_service_models(), [=] {
			stop();
			result->setText(tr::lng_nagram_service_testing(tr::now));
			request->models(service, crl::guard(box, [=](ServiceResult response) {
				if (response.error != ServiceError::None) {
					status(std::move(response));
					return;
				}
				const auto object = QJsonDocument::fromJson(response.body).object();
				const auto list = object.value(u"data"_q).toArray();
				auto models = QStringList();
				auto valid = object.value(u"data"_q).isArray()
					&& !list.isEmpty() && list.size() <= 2000;
				for (const auto &entry : list) {
					const auto value = entry.toObject().value(u"id"_q);
					const auto id = value.toString();
					valid = valid && value.isString() && !id.isEmpty()
						&& id.size() <= 256 && !id.contains('\n')
						&& !id.contains('\r') && !id.contains(QChar(0))
						&& QString::fromUtf8(id.toUtf8()) == id;
					models.push_back(id);
				}
				if (!valid) {
					status({ .error = ServiceError::Response });
					return;
				}
				models.removeDuplicates();
				models.sort();
				result->setText(tr::lng_nagram_service_models_about(tr::now));
				for (const auto &id : models) {
					const auto row = modelRows->add(object_ptr<Ui::SettingsButton>(
						modelRows, rpl::single(id), st::settingsButtonNoIcon));
					row->setClickedCallback([=] { chooseModel(id); box->closeBox(); });
				}
			}));
		});
	}
	if (service.kind == ServiceKind::Translation) {
		box->addButton(tr::lng_nagram_service_test_translation(), [=] {
			stop();
			result->setText(tr::lng_nagram_service_testing(tr::now));
			const auto call = BuildTranslationCall(
				service,
				{ u"Hello, world!"_q },
				u"zh"_q);
			request->json(service, call.body, call.query, crl::guard(box, [=](ServiceResult response) {
				if (response.error != ServiceError::None) {
					status(std::move(response));
					return;
				}
				const auto values = ParseTranslationResult(
					service,
					response.body,
					1);
				if (!values || values->front().size() > 16384) {
					status({ .error = ServiceError::Response });
				} else {
					result->setText(values->front());
				}
			}));
		});
	} else {
		box->addButton(tr::lng_nagram_service_test_audio(), [=] {
			stop();
			const auto expected = *generation;
			FileDialog::GetOpenPath(Core::App().getFileDialogParent(),
				tr::lng_nagram_service_test_audio(tr::now),
				tr::lng_nagram_service_audio_filter(tr::now),
				crl::guard(box, [=](FileDialog::OpenResult &&selected) {
					if (expected != *generation || selected.paths.isEmpty()) {
						return;
					}
					auto file = QFile(selected.paths.front());
					if (!file.open(QIODevice::ReadOnly)) {
						result->setText(tr::lng_nagram_service_audio_read_error(tr::now));
						return;
					}
					const auto bytes = file.read(24 * 1024 * 1024 + 1);
					const auto name = QFileInfo(file).fileName();
					file.close();
					box->uiShow()->showBox(Ui::MakeConfirmBox({
						.text = tr::lng_nagram_service_audio_upload(tr::now)
							+ u"\n\n"_q + name + u"\n"_q + ServiceEndpoint(service).toDisplayString(),
						.confirmed = crl::guard(box, [=](Fn<void()> close) {
							close();
							if (expected != *generation) {
								return;
							}
							result->setText(tr::lng_nagram_service_testing(tr::now));
							request->audio(service, bytes, name,
								crl::guard(box, [=](ServiceResult response) {
									if (response.error != ServiceError::None) {
										status(std::move(response));
										return;
									}
									const auto text = QJsonDocument::fromJson(response.body).object().value(u"text"_q).toString();
									if (text.isEmpty() || text.size() > 16384
										|| text.contains(QChar(0))
										|| QString::fromUtf8(text.toUtf8()) != text) {
										status({ .error = ServiceError::Response });
									} else {
										result->setText(text);
									}
								}));
						}),
					}));
				}));
		});
	}
	box->addButton(tr::lng_cancel(), [=] {
		stop();
		box->closeBox();
	});
}

void ServiceBox(
		not_null<Ui::GenericBox*> box,
		QJsonObject current,
		ServiceDefinition original,
		Fn<void(QJsonObject)> saved) {
	box->setTitle(tr::lng_nagram_service_edit());

	const auto add = [&](rpl::producer<QString> title,
			const QString &value, bool multiline = false) {
		box->addRow(object_ptr<Ui::FlatLabel>(box, std::move(title), st::boxLabel));
		return box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			multiline ? Ui::InputField::Mode::MultiLine : Ui::InputField::Mode::SingleLine,
			rpl::single(QString()),
			value));
	};
	const auto name = add(tr::lng_nagram_service_name(), original.name);
	const auto url = add(tr::lng_nagram_service_url(), original.baseUrl.toString());
	const auto endpoint = add(tr::lng_nagram_service_endpoint(), original.endpoint);
	const auto openai = LlmProtocol(original.protocol);
	const auto translation = original.kind == ServiceKind::Translation;
	const auto model = openai ? add(tr::lng_nagram_service_model(), original.model) : nullptr;
	const auto option = (original.protocol == u"deepl"_q)
		? add(tr::lng_nagram_service_formality(), original.model)
		: (original.protocol == u"microsoft"_q)
		? add(tr::lng_nagram_service_region(), original.model)
		: (original.protocol == u"yandex"_q)
		? add(tr::lng_nagram_service_folder(), original.model)
		: nullptr;
	const auto system = openai && translation
		? add(tr::lng_nagram_service_system_prompt(), original.systemPrompt, true) : nullptr;
	const auto prompt = openai ? add(tr::lng_nagram_service_prompt(), original.prompt, true) : nullptr;
	const auto summary = openai && translation
		? add(
			tr::lng_nagram_service_summary_prompt(),
			original.summaryPrompt,
			true)
		: nullptr;
	const auto language = !translation ? add(tr::lng_nagram_service_language(), original.language) : nullptr;
	const auto temperature = openai ? add(tr::lng_nagram_service_temperature(),
		original.temperature ? QString::number(*original.temperature) : QString()) : nullptr;
	const auto useKey = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_nagram_service_use_key(), st::settingsButtonNoIcon));
	useKey->toggleOn(rpl::single(original.useKey));
	const auto enabled = box->lifetime().make_state<bool>(original.useKey);
	useKey->toggledChanges() | rpl::on_next([=](bool value) {
		*enabled = value;
	}, box->lifetime());
	const auto keyRow = box->addRow(object_ptr<Ui::RpWidget>(box));
	keyRow->resize(keyRow->width(), st::defaultInputField.heightMin);
	const auto key = Ui::CreateChild<Ui::PasswordInput>(
		keyRow, st::defaultInputField, tr::lng_nagram_service_key());
	keyRow->widthValue() | rpl::on_next([=](int width) {
		key->resize(width, key->height());
	}, keyRow->lifetime());
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_service_edit_about(), st::boxLabel));
	box->addButton(tr::lng_settings_save(), [=] {
		if (!Current(box, current)) {
			return;
		}
		auto service = original;
		service.name = name->getLastText().trimmed();
		service.baseUrl = QUrl(url->getLastText().trimmed(), QUrl::StrictMode);
		service.endpoint = endpoint->getLastText().trimmed();
		service.model = model
			? model->getLastText().trimmed()
			: option
			? option->getLastText().trimmed()
			: QString();
		service.systemPrompt = system ? system->getLastText() : QString();
		service.prompt = prompt ? prompt->getLastText() : QString();
		service.summaryPrompt = summary ? summary->getLastText() : QString();
		service.language = language ? language->getLastText().trimmed() : QString();
		service.useKey = *enabled;
		service.temperature = std::nullopt;
		if (temperature && !temperature->getLastText().trimmed().isEmpty()) {
			auto ok = false;
			service.temperature = temperature->getLastText().trimmed().toDouble(&ok);
			if (!ok) {
				temperature->showError();
				return;
			}
		}
		const auto value = SerializeService(service);
		if (!ParseService(value)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		auto updated = current;
		auto instances = updated.value(u"instances"_q).toArray();
		auto found = false;
		for (auto i = 0; i != instances.size(); ++i) {
			if (instances[i].toObject().value(u"id"_q) == service.id) {
				instances[i] = value;
				found = true;
				break;
			}
		}
		if (!found) {
			instances.push_back(value);
		}
		updated.insert(u"instances"_q, instances);
		auto secret = key->getLastText().toUtf8();
		if (service.useKey) {
			const auto account = CredentialAccount(service);
			const auto error = !secret.isEmpty()
				? WriteCredential(account, secret)
				: ReadCredential(account).error;
			if (error != CredentialError::None) {
				secret.fill('\0');
				key->showError();
				box->showToast(tr::lng_nagram_service_key_error(tr::now));
				return;
			}
		}
		secret.fill('\0');
		if (!SetServices(updated)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		Core::App().saveSettingsDelayed();
		const auto oldAccount = CredentialAccount(original);
		if (found && oldAccount != CredentialAccount(service)
			&& !CredentialUsed(updated, oldAccount)
			&& RemoveServiceCredential(original) != CredentialError::None) {
			box->uiShow()->showToast(tr::lng_nagram_service_key_cleanup(tr::now));
		}
		saved(updated);
		box->closeBox();
	});
	if (FindService(current, original.id)) {
		box->addButton(tr::lng_nagram_service_test(), [=] {
			if (!Current(box, current)) {
				return;
			}
			box->uiShow()->showBox(Box(ServiceTestBox, original,
				crl::guard(box, [=](QString id) {
					if (model) {
						model->setTextWithTags({ std::move(id) });
					}
				})));
		});
		box->addButton(tr::lng_box_delete(), [=] {
			box->uiShow()->showBox(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_service_delete_confirm(tr::now),
				.confirmed = crl::guard(box, [=](Fn<void()> close) {
					if (!Current(box, current)) {
						return;
					}
					auto updated = current;
					auto instances = QJsonArray();
					for (const auto &value : current.value(u"instances"_q).toArray()) {
						if (value.toObject().value(u"id"_q) != original.id) {
							instances.push_back(value);
						}
					}
					updated.insert(u"instances"_q, instances);
					for (const auto &key : {
							u"translation"_q,
							u"transcription"_q,
							u"summary"_q }) {
						if (updated.value(key) == original.id) {
							updated.insert(key, QString());
						}
					}
					const auto account = CredentialAccount(original);
					if (!SetServices(updated)) {
						box->showToast(tr::lng_nagram_service_invalid(tr::now));
						return;
					}
					Core::App().saveSettingsDelayed();
					if (!CredentialUsed(updated, account)
						&& RemoveServiceCredential(original) != CredentialError::None) {
						box->uiShow()->showToast(
							tr::lng_nagram_service_key_cleanup(tr::now));
					}
					saved(updated);
					close();
					box->closeBox();
				}),
				.confirmText = tr::lng_box_delete(),
			}));
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

QString NewId() {
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void PresetsBox(
		not_null<Ui::GenericBox*> box,
		Fn<void(ServiceDefinition)> create) {
	box->setTitle(tr::lng_nagram_service_presets());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_service_presets_about(), st::boxLabel));
	for (const auto &preset : ServicePresets()) {
		const auto name = QString::fromUtf16(preset.name);
		const auto row = box->addRow(object_ptr<Ui::SettingsButton>(
			box,
			(preset.kind == ServiceKind::Translation)
				? tr::lng_nagram_service_preset_translation(
					lt_name, rpl::single(name))
				: tr::lng_nagram_service_preset_transcription(
					lt_name, rpl::single(name)),
			st::settingsButtonNoIcon), style::margins());
		row->setClickedCallback([=] {
			create(ServiceFromPreset(preset, NewId(), NewId()));
			box->closeBox();
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ServicesBox(not_null<Ui::GenericBox*> box, QJsonObject initial) {
	box->setTitle(tr::lng_nagram_services());

	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_services_about(), st::boxLabel));
	const auto state = box->lifetime().make_state<rpl::variable<QJsonObject>>(initial);
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto changed = crl::guard(box, [=](QJsonObject value) {
		crl::on_main(box, [=] { *state = value; });
	});
	state->value() | rpl::on_next([=](const QJsonObject &current) {
		rows->clear();
		const auto add = [&](QString title, Fn<void()> click) {
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(std::move(title)), st::settingsButtonNoIcon));
			row->setClickedCallback(std::move(click));
			return row;
		};
		for (const auto &value : current.value(u"instances"_q).toArray()) {
			const auto service = ParseService(value.toObject());
			if (service) {
				add(service->name, [=] {
					box->uiShow()->showBox(Box(ServiceBox, current, *service, changed));
				});
			}
		}
		const auto create = crl::guard(box, [=](ServiceDefinition service) {
			box->uiShow()->showBox(
				Box(ServiceBox, current, std::move(service), changed));
		});
		add(tr::lng_nagram_service_presets(tr::now), [=] {
			box->uiShow()->showBox(Box(PresetsBox, create));
		});
		add(tr::lng_nagram_service_add_custom(tr::now), [=] {
			create(ServiceDefinition{
				.id = NewId(),
				.protocol = u"openai"_q,
				.endpoint = u"chat/completions"_q,
				.credentialRef = NewId(),
			});
		});
	}, box->lifetime());
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = ServicesSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_services,
	.icon = &st::menuIconTranslate,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto quickTranslate = builder.addButton({
		.id = u"regram/services/quick-translate"_q,
		.title = tr::lng_regram_quick_translate(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(kQuickTranslateButton),
		.keywords = { u"translate"_q, u"quick"_q, u"iOS"_q },
	});
	if (quickTranslate) {
		quickTranslate->toggledChanges() | rpl::on_next([](bool enabled) {
			Expects(ForDevice().Set(kQuickTranslateButton, enabled));
		}, quickTranslate->lifetime());
	}
	builder.addDividerText(tr::lng_regram_quick_translate_about());
	builder.addButton({
		.id = u"regram/services/skip-languages"_q,
		.title = tr::lng_translate_settings_choose(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Ui::EditSkipTranslationLanguages()); },
		.keywords = { u"translate"_q, u"skip"_q, u"language"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/services/translation"_q,
		.title = tr::lng_nagram_service_translation(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::combine(
			ForDevice().Value(kServicesConfig),
			AyuSettings::getInstance().translationProviderValue()
		) | rpl::map([] {
			return TranslationSelectionName(Services());
		}),
		.onClick = [=] { controller->show(Box(TranslationSourceBox)); },
		.keywords = { u"translation"_q, u"system"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/services/transcription"_q,
		.title = tr::lng_nagram_service_transcription(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(kServicesConfig)
			| rpl::map([](const QByteArray &) {
				return TranscriptionSelectionName(Services());
			}),
		.onClick = [=] { controller->show(Box(TranscriptionSourceBox)); },
		.keywords = { u"transcription"_q, u"voice"_q },
	});
	const auto contextButton = builder.addButton({
		.id = u"regram/nagram/services/translation-context"_q,
		.title = tr::lng_nagram_service_use_context(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(kTranslationContext),
		.keywords = { u"context"_q, u"LLM"_q, u"translation"_q },
	});
	if (contextButton) {
		contextButton->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(kTranslationContext, value));
		}, contextButton->lifetime());
	}
	builder.addDividerText(tr::lng_nagram_service_use_context_about(
		lt_amount,
		rpl::single(QString::number(kContextMessages)),
		lt_each,
		rpl::single(QString::number(kContextEach)),
		lt_total,
		rpl::single(QString::number(kContextTotal))));
#ifdef Q_OS_MAC
	const auto aiStatus = SystemAiAvailability();
	const auto aiButton = builder.addButton({
		.id = u"regram/nagram/services/system-ai"_q,
		.title = tr::lng_nagram_system_ai(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(kPreferSystemAi),
		.keywords = { u"Apple Intelligence"_q, u"AI"_q },
	});
	if (aiButton) {
		aiButton->setDisabled(aiStatus != 0
			&& !ForDevice().Get(kPreferSystemAi));
		aiButton->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(kPreferSystemAi, value));
		}, aiButton->lifetime());
	}
	builder.addDividerText(tr::lng_nagram_system_ai_about());
	if (aiStatus != 0) {
		builder.addDividerText(rpl::single(SystemAiStatusText(aiStatus)));
	}
#endif // Q_OS_MAC
	builder.addButton({
		.id = u"regram/nagram/services/instances"_q,
		.title = tr::lng_nagram_services(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			const auto current = Services();
			if (!current) {
				controller->show(Ui::MakeInformBox(tr::lng_nagram_service_invalid(tr::now)));
				return;
			}
			controller->show(Box(ServicesBox, *current));
		},
		.keywords = { u"Regram"_q, u"LLM"_q, u"API"_q },
	});
	builder.addDividerText(tr::lng_nagram_services_about());
	builder.addButton({
		.id = u"regram/nagram/services/summary"_q,
		.title = tr::lng_nagram_summary_service(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(kServicesConfig)
			| rpl::map([](const QByteArray &) {
				return SummarySelectionName(Services());
			}),
		.onClick = [=] { controller->show(Box(SummarySourceBox)); },
		.keywords = { u"summary"_q, u"summarize"_q, u"LLM"_q },
	});
	builder.addDividerText(tr::lng_nagram_summary_service_about(
		lt_amount,
		rpl::single(QString::number(kSummaryMessages)),
		lt_total,
		rpl::single(QString::number(kSummaryLength))));
	builder.addSubsectionTitle({
		.id = u"regram/nagram/services/chat-translation"_q,
		.title = tr::lng_nagram_chat_translation(),
		.keywords = { u"auto translate"_q, u"translation"_q },
	});
	const auto session = builder.session();
	builder.addButton({
		.id = u"regram/nagram/services/auto-translate"_q,
		.title = tr::lng_nagram_auto_translate(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(AutoTranslate::kDeviceMode)
			| rpl::map([](int value) {
				return AutoTranslate::ModeLabel(
					AutoTranslate::ValidMode(value)
						? AutoTranslate::Mode(value)
						: AutoTranslate::Mode::Inherit,
					tr::lng_nagram_auto_translate_inherit(tr::now));
			}),
		.onClick = [=] {
			controller->show(Box(AutoTranslate::DeviceModeBox));
		},
		.keywords = { u"auto translate"_q, u"translation"_q },
	});
	builder.addDividerText(AutoTranslate::AboutValue(session));
	builder.addButton({
		.id = u"regram/nagram/services/auto-translate-account"_q,
		.title = tr::lng_nagram_auto_translate_account(),
		.st = &st::settingsButtonNoIcon,
		.label = ForAccount(session).Value(AutoTranslate::kAccountMode)
			| rpl::map([](int value) {
				return AutoTranslate::ModeLabel(
					AutoTranslate::ValidMode(value)
						? AutoTranslate::Mode(value)
						: AutoTranslate::Mode::Inherit,
					tr::lng_nagram_auto_translate_account_inherit(tr::now));
			}),
		.onClick = [=] {
			controller->show(Box(AutoTranslate::AccountModeBox, session));
		},
		.keywords = { u"auto translate"_q, u"account"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/services/auto-translate-chats"_q,
		.title = tr::lng_nagram_auto_translate_chats(),
		.st = &st::settingsButtonNoIcon,
		.label = ForAccount(session).Value(AutoTranslate::kChats)
			| rpl::map([](const QByteArray &raw) {
				return QString::number(AutoTranslate::ParseChats(
					raw).value_or(AutoTranslate::ChatModes()).size());
			}),
		.onClick = [=] {
			controller->show(Box(AutoTranslate::ChatsBox, session));
		},
		.keywords = { u"auto translate"_q, u"chat"_q },
	});
	builder.addDividerText(tr::lng_nagram_auto_translate_chats_about());
	const auto chatButton = builder.addButton({
		.id = u"regram/nagram/services/chat-translation-service"_q,
		.title = tr::lng_nagram_chat_translation_service(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(kChatTranslationUseService),
		.keywords = { u"auto translate"_q, u"translate bar"_q, u"LLM"_q },
	});
	if (chatButton) {
		chatButton->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(kChatTranslationUseService, value));
		}, chatButton->lifetime());
	}
	builder.addDividerText(ForDevice().Value(kServicesConfig)
		| rpl::map([](const QByteArray &) {
			return tr::lng_nagram_chat_translation_service_about(
				tr::now,
				lt_name,
				TranslationSelectionName(Services()));
		}));
	const auto sendButton = builder.addButton({
		.id = u"regram/nagram/services/send-translation"_q,
		.title = tr::lng_nagram_send_translation(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(SendTranslation::kEnabled),
		.keywords = { u"translate"_q, u"send"_q, u"draft"_q },
	});
	if (sendButton) {
		sendButton->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(SendTranslation::kEnabled, value));
		}, sendButton->lifetime());
	}
	builder.addButton({
		.id = u"regram/nagram/services/send-translation-chats"_q,
		.title = tr::lng_nagram_send_translation_chats(),
		.st = &st::settingsButtonNoIcon,
		.label = SendTranslation::ChatsCountValue(
			session
		) | rpl::map([](int count) { return QString::number(count); }),
		.onClick = [=] {
			const auto weak = base::make_weak(session);
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_send_translation_clear(),
				.confirmed = [=](Fn<void()> close) {
					if (const auto strong = weak.get()) {
						SendTranslation::ClearChats(strong);
					}
					close();
				},
			}));
		},
		.keywords = { u"translate"_q, u"send"_q, u"chats"_q },
	});
	builder.addDividerText(tr::lng_nagram_send_translation_about());
});

const SectionBuildMethod ServicesSection::kBuild = kMeta.build;

} // namespace

Settings::Type ServicesId() {
	return ServicesSection::Id();
}

} // namespace Nagram
