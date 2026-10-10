#include "regram/nagram/settings/config.h"

#include "core/application.h"
#include "core/file_utilities.h"
#include "core/version.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "lang_auto_counts.h"
#include "logs.h"
#include "regram/ayu/ui/settings/settings_main.h"
#include "regram/nagram/core/diagnostics.h"
#include "regram/nagram/core/exchange.h"
#include "regram/nagram/core/language.h"
#include "regram/nagram/settings/chats.h"
#include "regram/nagram/settings/cloud_sync.h"
#include "regram/nagram/settings/compose.h"
#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/interface.h"
#include "regram/nagram/settings/media.h"
#include "regram/nagram/settings/menu.h"
#include "regram/nagram/settings/messages.h"
#include "regram/nagram/settings/network.h"
#include "regram/nagram/settings/privacy.h"
#include "regram/nagram/settings/restart.h"
#include "regram/nagram/settings/rules.h"
#include "regram/nagram/settings/services.h"
#include "regram/settings/advanced_model.h"
#include "settings.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtGui/QClipboard>

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

constexpr auto kMaximumImportBytes = 8 * 1024 * 1024;

class ConfigSection final : public Section<ConfigSection> {
public:
	ConfigSection(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_config_title();
	}

	static const SectionBuildMethod kBuild;
};

QString Title(const OptionInfo &info) {
	const auto key = QByteArray(info.titleKey.data(), info.titleKey.size());
	const auto index = Lang::GetKeyIndex(QLatin1String(key));
	return (index == Lang::kKeysCount)
		? QString::fromUtf8(key)
		: LocalizedValue(Lang::GetInstance(), index);
}

Settings::Type CategorySection(Category category) {
	switch (category) {
	case Category::Interface: return InterfaceId();
	case Category::Chats: return ChatsId();
	case Category::Messages: return MessagesId();
	case Category::Compose: return ComposeId();
	case Category::Menu: return MenuId();
	case Category::Media: return MediaId();
	case Category::Privacy: return PrivacyId();
	case Category::Services: return ServicesId();
	case Category::Rules: return RulesId();
	case Category::Network: return NetworkId();
	}
	Unexpected("Invalid Nagram option category.");
}

void AddText(not_null<Ui::GenericBox*> box, const QString &text) {
	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box, text, st::boxLabel));
	label->setSelectable(true);
	label->setBreakEverywhere(true);
}

QString ValueText(const OptionInfo &info, const QByteArray &raw) {
	const auto value = raw.isEmpty() ? info.fallbackRaw : raw;
	if (info.type == OptionInfo::ValueType::Boolean) {
		return (value == "1")
			? tr::lng_nagram_config_on(tr::now)
			: tr::lng_nagram_config_off(tr::now);
	}
	auto result = (info.type == OptionInfo::ValueType::String)
		? QString::fromUtf8(value.mid(1))
		: QString::fromUtf8(value);
	constexpr auto kPreviewLength = 120;
	if (result.size() > kPreviewLength) {
		result.truncate(kPreviewLength);
		if (result.back().isHighSurrogate()) {
			result.chop(1);
		}
		result += QChar(0x2026);
	}
	return result;
}

void ShowModified(not_null<Window::SessionController*> controller) {
	const auto exported = Exchange::Export(ForDevice(), RegisteredOptions());
	const auto values = QJsonDocument::fromJson(exported.data
		).object().value(u"options"_q).toObject();
	const auto entries = SearchRegistry::Instance().collectAll(
		&controller->session());
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_modified());
		AddText(box, tr::lng_nagram_config_modified_about(tr::now));
		if (values.isEmpty()) {
			AddText(box, tr::lng_nagram_config_no_changes(tr::now));
		}
		for (auto i = values.begin(); i != values.end(); ++i) {
			const auto encodedKey = i.key().toUtf8();
			const auto info = RegisteredOptions().Find(std::string_view(
				encodedKey.constData(), encodedKey.size()));
			if (!info) {
				continue;
			}
			const auto title = Title(*info);
			const auto fallback = (info->key == Regram::Settings::kAdvancedSettings.key)
				? ::Settings::AyuMainId()
				: CategorySection(info->category);
			const auto found = ranges::find_if(entries, [&](const SearchEntry &entry) {
				return entry.section == fallback && entry.title == title;
			});
			const auto section = (found != entries.end())
				? found->section : fallback;
			const auto id = (found != entries.end()) ? found->id : QString();
			const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
				box, rpl::single(title), st::settingsButtonNoIcon));
			button->setClickedCallback(crl::guard(controller, [=] {
				box->closeBox();
				if (!id.isEmpty()) {
					controller->setHighlightControlId(id);
				}
				controller->showSettings(section);
			}));
		}
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void Export(not_null<Window::SessionController*> controller) {
	const auto exported = Exchange::Export(ForDevice(), RegisteredOptions());
	if (!exported.invalidKeys.isEmpty()) {
		controller->showToast(tr::lng_nagram_config_invalid(tr::now));
		return;
	}
	FileDialog::GetWritePath(
		Core::App().getFileDialogParent(),
		tr::lng_nagram_config_export(tr::now),
		tr::lng_nagram_config_file_filter(tr::now),
		u"regram-settings.json"_q,
		crl::guard(controller, [=](QString &&path) {
			if (path.isEmpty()) {
				return;
			}
			auto file = QSaveFile(path);
			if (!file.open(QIODevice::WriteOnly)
				|| file.write(exported.data) != exported.data.size()
				|| !file.commit()) {
				controller->showToast(tr::lng_nagram_config_write_error(tr::now));
			} else {
				controller->showToast(tr::lng_nagram_config_exported(tr::now));
			}
		}));
}

void Import(not_null<Window::SessionController*> controller) {
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		tr::lng_nagram_config_import(tr::now),
		tr::lng_nagram_config_file_filter(tr::now),
		crl::guard(controller, [=](FileDialog::OpenResult &&result) {
			if (!result.paths.isEmpty()) {
				auto file = QFile(result.paths.front());
				if (!file.open(QIODevice::ReadOnly)) {
					controller->showToast(tr::lng_nagram_config_read_error(tr::now));
					return;
				}
				ShowImport(controller, file.read(kMaximumImportBytes + 1));
			} else if (!result.remoteContent.isEmpty()) {
				ShowImport(controller, result.remoteContent);
			}
		}));
}

void CopyDiagnostics(not_null<Window::SessionController*> controller) {
	const auto &registry = RegisteredOptions();
	const auto exported = Exchange::Export(ForDevice(), registry);
	const auto modified = QJsonDocument::fromJson(exported.data
		).object().value(u"options"_q).toObject().size();
	const auto managed = ranges::count_if(registry.All(), [](const OptionInfo &info) {
		return info.scope == Scope::Device;
	});
	const auto report = tr::lng_nagram_config_diagnostic_report(
		tr::now,
		lt_version, QString::fromLatin1(AppVersionStr),
		lt_amount, QString::number(managed),
		lt_value, QString::number(modified),
		lt_error, QString::number(exported.invalidKeys.size()));
	QGuiApplication::clipboard()->setText(report);
	controller->showToast(tr::lng_nagram_config_copy_report(tr::now));
}

const auto kMeta = BuildHelper({
	.id = ConfigSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_config_title,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addDividerText(tr::lng_nagram_config_scope());
	builder.addButton({
		.id = u"regram/nagram/config/modified"_q,
		.title = tr::lng_nagram_config_modified(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { ShowModified(controller); },
	});
	builder.addButton({
		.id = u"regram/nagram/config/export"_q,
		.title = tr::lng_nagram_config_export(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { Export(controller); },
	});
	builder.addButton({
		.id = u"regram/nagram/config/import"_q,
		.title = tr::lng_nagram_config_import(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { Import(controller); },
	});
	builder.addButton({
		.id = u"regram/nagram/config/diagnostics"_q,
		.title = tr::lng_nagram_config_diagnostics(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { CopyDiagnostics(controller); },
	});
	const auto rpc = builder.addButton({
		.id = u"regram/nagram/config/rpc-errors"_q,
		.title = tr::lng_nagram_show_rpc_errors(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(kShowRpcErrors),
		.keywords = { u"RPC"_q, u"error"_q },
	});
	if (rpc) {
		rpc->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(kShowRpcErrors, value));
		}, rpc->lifetime());
	}
	builder.addButton({
		.id = u"regram/nagram/config/debug-logs"_q,
		.title = tr::lng_nagram_debug_logs(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(Logs::DebugEnabled()
			? tr::lng_nagram_config_on(tr::now)
			: tr::lng_nagram_reading_off(tr::now)),
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = (Logs::DebugEnabled()
					? tr::lng_nagram_debug_logs_disable
					: tr::lng_nagram_debug_logs_enable)(),
				.confirmed = [] { Core::App().switchDebugMode(); },
			}));
		},
		.keywords = { u"log"_q, u"debug"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/config/show-logs"_q,
		.title = tr::lng_nagram_show_logs(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [] { File::ShowInFolder(cWorkingDir() + u"log.txt"_q); },
		.keywords = { u"log"_q, u"folder"_q },
	});
	builder.addDividerText(tr::lng_nagram_logs_note());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/config/sync"_q,
		.title = tr::lng_nagram_sync_title(),
		.keywords = { u"backup"_q, u"sync"_q, u"cloud"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/config/sync-backup"_q,
		.title = tr::lng_nagram_sync_backup(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { BackupToCloud(controller); },
		.keywords = { u"backup"_q, u"saved"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/config/sync-restore"_q,
		.title = tr::lng_nagram_sync_restore(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { RestoreFromCloud(controller); },
		.keywords = { u"restore"_q, u"saved"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/config/sync-now"_q,
		.title = tr::lng_nagram_sync_now(),
		.st = &st::settingsButtonNoIcon,
		.label = CloudSyncStatus(builder.session()),
		.onClick = [=] { SyncWithCloud(controller); },
		.keywords = { u"sync"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/config/sync-delete"_q,
		.title = tr::lng_nagram_sync_delete_remote(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { DeleteCloudBackup(controller); },
		.keywords = { u"backup"_q, u"delete"_q },
	});
	builder.addDividerText(tr::lng_nagram_sync_about());
	const auto session = builder.session();
	const auto automatic = builder.addButton({
		.id = u"regram/nagram/config/sync-auto"_q,
		.title = tr::lng_nagram_sync_auto(),
		.st = &st::settingsButtonNoIcon,
		.toggled = CloudSyncAutoValue(session),
		.keywords = { u"backup"_q, u"automatic"_q },
	});
	if (automatic) {
		automatic->toggledChanges(
		) | rpl::on_next([=](bool value) {
			SetCloudSyncAuto(session, value);
		}, automatic->lifetime());
	}
	builder.addDividerText(tr::lng_nagram_sync_auto_about());
});

const SectionBuildMethod ConfigSection::kBuild = kMeta.build;

} // namespace

Settings::Type ConfigId() {
	return ConfigSection::Id();
}

void ShowImport(
		not_null<Window::SessionController*> controller,
		const QByteArray &bytes,
		ExchangeTarget target,
		Fn<void()> applied) {
	const auto plan = Exchange::PlanImport(
		ForDevice(), RegisteredOptions(), bytes, target);
	if (!plan.error.isEmpty()) {
		controller->showToast(plan.error);
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_preview());
		AddText(box, tr::lng_nagram_config_import_about(tr::now));
		AddText(box, tr::lng_nagram_config_changes(
			tr::now, lt_amount, QString::number(plan.changes.size())));
		for (const auto &change : plan.changes) {
			const auto encodedKey = change.key.toUtf8();
			const auto info = RegisteredOptions().Find(std::string_view(
				encodedKey.constData(), encodedKey.size()));
			if (info) {
				AddText(box, Title(*info) + u"\n"_q
					+ ValueText(*info, change.before)
					+ u" → "_q + ValueText(*info, change.after));
			}
		}
		if (plan.adjustedKeys.contains(u"nagram.inlineBotRules"_q)) {
			AddText(box, tr::lng_nagram_inline_import_disabled(tr::now));
		}
		if (!plan.skippedKeys.isEmpty()) {
			AddText(box, tr::lng_nagram_config_unknown(
				tr::now, lt_amount, QString::number(plan.skippedKeys.size()))
				+ u"\n"_q + plan.skippedKeys.mid(0, 20).join('\n'));
		}
		if (!plan.changes.empty()) {
			box->addButton(tr::lng_nagram_config_apply(),
				crl::guard(controller, [=] {
					const auto result = Exchange::Apply(
						ForDevice(), RegisteredOptions(), plan);
					if (!result.applied) {
						box->showToast(result.error);
						return;
					}
					box->closeBox();
					controller->showToast(tr::lng_nagram_config_imported(tr::now));
					if (applied) {
						applied();
					}
					if (ranges::any_of(plan.changes, [](const ExchangeChange &change) {
						const auto key = change.key.toUtf8();
						const auto info = RegisteredOptions().Find(std::string_view(
							key.constData(), key.size()));
						return info && (info->flags
							& static_cast<unsigned>(Flag::RequiresRestart));
					})) {
						ShowRestartPrompt(controller);
					}
				}));
		}
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram
