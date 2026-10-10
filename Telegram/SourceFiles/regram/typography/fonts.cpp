#include "regram/typography/fonts.h"

#include "regram/typography/font_data.h"

#include "base/qt/qt_common_adapters.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/nagram/settings/restart.h"
#include "storage/localstorage.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QFontDatabase>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <map>

#include "styles/style_layers.h"
#include "styles/style_nagram_interface.h"
#include "styles/style_settings.h"

namespace Regram::Typography {
namespace {

constexpr auto kMaxFontBytes = 64 * 1024 * 1024;
constexpr auto kCjkFamilyKey = "regram.cjkFontFamily";

enum class Target { Interface, Chinese, Code };

struct FontFile {
	QString name;
	QUrl url;
	QByteArray hash;
	qint64 bytes = 0;
};

struct Family {
	QString id;
	QString title;
	std::vector<FontFile> files;
};

struct Installed {
	QString id;
	QString title;
	QStringList families;
	QStringList files;
	bool imported = false;
};

struct BoxState {
	QNetworkAccessManager network;
	std::unique_ptr<QTemporaryDir> stage;
	QPointer<QNetworkReply> reply;
	Family family;
	QByteArray bytes;
	Fn<void()> refresh;
	int nextFile = 0;
	bool busy = false;
	rpl::variable<QString> status;
};

QString Root() {
	return cWorkingDir() + u"tdata/regram/fonts/"_q;
}

bool Save(const QString &path, const QByteArray &bytes) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QSaveFile(path);
	return file.open(QIODevice::WriteOnly)
		&& file.write(bytes) == bytes.size() && file.commit();
}

QByteArray Read(const QString &path) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly) ? file.read(kMaxFontBytes + 1) : QByteArray();
}

const std::vector<Family> &Catalog() {
	static const auto result = [] {
		const auto object = QJsonDocument::fromJson(Read(u":/regram/fonts/catalog.json"_q)).object();
		auto files = std::map<QString, FontFile>();
		for (const auto entry : object.value(u"files"_q).toArray()) {
			const auto file = entry.toObject();
			const auto name = file.value(u"file"_q).toString();
			files.emplace(name, FontFile{
				.name = name,
				.url = QUrl(file.value(u"url"_q).toString()),
				.hash = file.value(u"sha256"_q).toString().toLatin1(),
				.bytes = file.value(u"bytes"_q).toInteger(),
			});
		}
		auto result = std::vector<Family>();
		for (const auto entry : object.value(u"families"_q).toArray()) {
			const auto family = entry.toObject();
			auto parsed = Family{
				.id = family.value(u"id"_q).toString(),
				.title = family.value(u"title"_q).toString(),
			};
			for (const auto name : family.value(u"files"_q).toArray()) {
			const auto found = files.find(name.toString());
			if (found != files.end()) {
					parsed.files.push_back(found->second);
				}
			}
			result.push_back(std::move(parsed));
		}
		return result;
	}();
	return result;
}

std::vector<Installed> InstalledFonts() {
	auto result = std::vector<Installed>();
	const auto entries = QJsonDocument::fromJson(Read(Root() + u"index.json"_q)).array();
	for (const auto entry : entries) {
		const auto object = entry.toObject();
		auto parsed = Installed{
			.id = object.value(u"id"_q).toString(),
			.title = object.value(u"title"_q).toString(),
			.imported = object.value(u"imported"_q).toBool(),
		};
		for (const auto family : object.value(u"families"_q).toArray()) {
			parsed.families.push_back(family.toString());
		}
		for (const auto file : object.value(u"files"_q).toArray()) {
			const auto path = file.toString();
			if (!path.contains(u'\\') && !path.contains(u".."_q) && !QDir::isAbsolutePath(path)) {
				parsed.files.push_back(path);
			}
		}
		if (!parsed.id.isEmpty() && !parsed.families.empty() && !parsed.files.empty()) {
			result.push_back(std::move(parsed));
		}
	}
	return result;
}

bool SaveIndex(const std::vector<Installed> &entries) {
	auto array = QJsonArray();
	for (const auto &entry : entries) {
		array.push_back(QJsonObject{
			{ u"id"_q, entry.id },
			{ u"title"_q, entry.title },
			{ u"families"_q, QJsonArray::fromStringList(entry.families) },
			{ u"files"_q, QJsonArray::fromStringList(entry.files) },
			{ u"imported"_q, entry.imported },
		});
	}
	return Save(Root() + u"index.json"_q, QJsonDocument(array).toJson());
}

QStringList RegisterFont(const QString &path) {
	static auto registered = std::map<QString, QStringList>();
	const auto found = registered.find(path);
	if (found != registered.end()) {
		return found->second;
	}
	const auto id = QFontDatabase::addApplicationFontFromData(NativeFontData(Read(path)));
	if (id < 0) {
		return {};
	}
	const auto families = QFontDatabase::applicationFontFamilies(id);
	registered.emplace(path, families);
	return families;
}

void ApplyFamily(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		const QString &family,
		Target target) {
	auto &settings = Core::App().settings();
	switch (target) {
	case Target::Interface: settings.setCustomFontFamily(family); break;
	case Target::Chinese: settings.writePref<QString>(kCjkFamilyKey, family); break;
	case Target::Code: AyuSettings::getInstance().setMonoFont(family); break;
	}
	Local::writeSettings();
	box->showToast(tr::lng_nagram_restart_required(tr::now));
	Nagram::ShowRestartPrompt(controller);
}

void ChooseFamily(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		QString family) {
	box->setTitle(rpl::single(family.isEmpty() ? tr::lng_font_default(tr::now) : family));
	const auto preview = box->addRow(object_ptr<Ui::RpWidget>(box));
	preview->resize(st::boxWidth, st::nagramFontPreviewHeight);
	preview->paintRequest() | rpl::on_next([=] {
		auto p = Painter(preview);
		auto font = st::boxLabel.style.font->f;
		if (!family.isEmpty()) {
			font.setFamily(family);
		}
		p.setFont(font);
		p.setPen(st::windowFg);
		p.drawText(preview->rect().marginsRemoved(QMargins(
			st::nagramFontPreviewPadding, st::nagramFontPreviewPadding,
			st::nagramFontPreviewPadding, st::nagramFontPreviewPadding)),
			Qt::TextWordWrap, u"Regram · Aa Bb 012345\n你好，世界！中文字体预览。"_q);
	}, preview->lifetime());
	const auto add = [&](Target target, rpl::producer<QString> title) {
		const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
			box, std::move(title), st::settingsButtonNoIcon));
		button->setClickedCallback([=] { ApplyFamily(box, controller, family, target); });
	};
	add(Target::Interface, tr::lng_regram_font_use_main());
	add(Target::Chinese, tr::lng_regram_font_use_chinese());
	add(Target::Code, tr::lng_regram_font_use_code());
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FailDownload(not_null<Ui::GenericBox*> box, not_null<BoxState*> state) {
	state->busy = false;
	state->status = QString();
	state->stage.reset();
	state->bytes.clear();
	box->showToast(tr::lng_regram_font_download_error(tr::now));
	state->refresh();
}

void NextDownload(not_null<Ui::GenericBox*> box, not_null<BoxState*> state) {
	if (state->nextFile == int(state->family.files.size())) {
		auto names = QStringList();
		auto installed = InstalledFonts();
		auto paths = QStringList();
		for (const auto &file : state->family.files) {
			const auto relative = u"cloud/"_q + state->family.id + u'/' + file.name;
			if (!Save(Root() + relative, Read(state->stage->filePath(file.name)))) {
				FailDownload(box, state);
				return;
			}
			const auto loaded = RegisterFont(Root() + relative);
			if (loaded.empty()) {
				FailDownload(box, state);
				return;
			}
			names.append(loaded);
			paths.push_back(relative);
		}
		names.removeDuplicates();
		std::erase_if(installed, [&](const auto &entry) {
			return entry.id == state->family.id;
		});
		installed.push_back({ state->family.id, state->family.title, names, paths, false });
		if (!SaveIndex(installed)) {
			FailDownload(box, state);
			return;
		}
		state->busy = false;
		state->status = QString();
		state->stage.reset();
		box->showToast(tr::lng_regram_font_downloaded(tr::now));
		state->refresh();
		return;
	}
	const auto file = state->family.files[state->nextFile];
	state->status = tr::lng_regram_font_progress(tr::now,
		lt_count, state->nextFile + 1,
		lt_amount, QString::number(state->family.files.size()));
	state->bytes.clear();
	auto request = QNetworkRequest(file.url);
	request.setTransferTimeout(30000);
	const auto reply = state->network.get(request);
	reply->setReadBufferSize(file.bytes + 1);
	state->reply = reply;
	QObject::connect(reply, &QNetworkReply::readyRead, box, [=] {
		state->bytes.append(reply->readAll());
		if (state->bytes.size() > file.bytes) {
			reply->abort();
		}
	});
	QObject::connect(reply, &QNetworkReply::finished, box, [=] {
		reply->deleteLater();
		state->bytes.append(reply->readAll());
		const auto valid = reply->error() == QNetworkReply::NoError
			&& reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
			&& state->bytes.size() == file.bytes
			&& QCryptographicHash::hash(state->bytes, QCryptographicHash::Sha256).toHex() == file.hash;
		if (!valid || !Save(state->stage->filePath(file.name), state->bytes)) {
			FailDownload(box, state);
			return;
		}
		++state->nextFile;
		NextDownload(box, state);
	});
}

void Download(not_null<Ui::GenericBox*> box, not_null<BoxState*> state, Family family) {
	if (state->busy) {
		return;
	}
	state->stage = std::make_unique<QTemporaryDir>();
	if (!state->stage->isValid() || family.files.empty()) {
		FailDownload(box, state);
		return;
	}
	state->busy = true;
	state->family = std::move(family);
	state->nextFile = 0;
	box->showToast(tr::lng_regram_font_downloading(tr::now));
	NextDownload(box, state);
}

void ImportFont(not_null<Ui::GenericBox*> box, not_null<BoxState*> state) {
	FileDialog::GetOpenPath(Core::App().getFileDialogParent(),
		tr::lng_regram_font_import(tr::now),
		u"Fonts (*.ttf *.otf *.ttc)"_q,
		crl::guard(box, [=](FileDialog::OpenResult &&result) {
			const auto bytes = result.paths.isEmpty() ? result.remoteContent : Read(result.paths.front());
			if (bytes.isEmpty() || bytes.size() > kMaxFontBytes) {
				box->showToast(tr::lng_regram_font_import_error(tr::now));
				return;
			}
			const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
			const auto relative = u"import/"_q + QString::fromLatin1(hash) + u".font"_q;
			if (!Save(Root() + relative, bytes)) {
				box->showToast(tr::lng_regram_font_import_error(tr::now));
				return;
			}
			const auto names = RegisterFont(Root() + relative);
			if (names.empty()) {
				QFile::remove(Root() + relative);
				box->showToast(tr::lng_regram_font_import_error(tr::now));
				return;
			}
			auto installed = InstalledFonts();
			const auto id = QString::fromLatin1(hash);
			std::erase_if(installed, [&](const auto &entry) {
				return entry.imported && entry.files.contains(relative);
			});
			for (auto index = 0; index != names.size(); ++index) {
				const auto &family = names[index];
				installed.push_back({ id + u'-' + QString::number(index),
					family, { family }, { relative }, true });
			}
			if (!SaveIndex(installed)) {
				box->showToast(tr::lng_regram_font_import_error(tr::now));
				return;
			}
			state->refresh();
		}));
}

bool RemoveFont(not_null<Ui::GenericBox*> box, Installed entry) {
	auto installed = InstalledFonts();
	std::erase_if(installed, [&](const auto &current) { return current.id == entry.id; });
	if (!SaveIndex(installed)) {
		box->showToast(tr::lng_regram_font_import_error(tr::now));
		return false;
	}
	auto &settings = Core::App().settings();
	if (entry.families.contains(settings.customFontFamily())) {
		settings.setCustomFontFamily({});
	}
	if (entry.families.contains(settings.readPref<QString>(kCjkFamilyKey))) {
		settings.writePref<QString>(kCjkFamilyKey, QString());
	}
	if (entry.families.contains(AyuSettings::getInstance().monoFont())) {
		AyuSettings::getInstance().setMonoFont({});
	}
	for (const auto &path : entry.files) {
		if (!ranges::any_of(installed, [&](const auto &current) {
			return current.files.contains(path);
		})) {
			QFile::remove(Root() + path);
		}
	}
	Local::writeSettings();
	return true;
}

void ManagedFontBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Installed entry,
		Fn<void()> refresh) {
	for (const auto &path : entry.files) {
		const auto loaded = RegisterFont(Root() + path);
		if (loaded.empty()) {
			box->showToast(tr::lng_regram_font_import_error(tr::now));
		}
	}
	box->setTitle(rpl::single(entry.title));
	for (const auto &family : entry.families) {
		const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
			box, rpl::single(family), st::settingsButtonNoIcon));
		button->setClickedCallback([=] {
			box->uiShow()->showBox(Box(ChooseFamily, controller, family));
		});
	}
	const auto title = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, tr::lng_regram_font_name(), entry.title));
	title->setMaxLength(128);
	box->addButton(tr::lng_settings_save(), [=] {
		if (title->getLastText().trimmed().isEmpty()) {
			title->showError();
			return;
		}
		auto installed = InstalledFonts();
		for (auto &current : installed) {
			if (current.id == entry.id) {
				current.title = title->getLastText().trimmed();
			}
		}
		if (SaveIndex(installed)) {
			refresh();
			box->closeBox();
		}
	});
	box->addButton(tr::lng_box_delete(), [=] {
		box->uiShow()->showBox(Ui::MakeConfirmBox({
			.text = tr::lng_regram_font_delete_confirm(),
			.confirmed = crl::guard(box, [=](Fn<void()> close) {
				if (!RemoveFont(box, entry)) {
					return;
				}
				refresh();
				close();
				box->closeBox();
				Nagram::ShowRestartPrompt(controller);
			}),
		}));
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void SystemFontsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_regram_font_system());
	const auto search = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, tr::lng_regram_font_search()));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto families = QFontDatabase::families();
	const auto refresh = [=] {
		rows->clear();
		const auto query = search->getLastText().trimmed();
		auto count = 0;
		for (const auto &family : families) {
			if (!family.contains(query, Qt::CaseInsensitive)) {
				continue;
			}
			const auto button = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(family), st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				box->uiShow()->showBox(Box(ChooseFamily, controller, family));
			});
			if (++count == 100) {
				break;
			}
		}
	};
	search->changes() | rpl::on_next(refresh, box->lifetime());
	refresh();
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

void Start() {
	auto &settings = Core::App().settings();
	const auto active = QStringList{
		settings.customFontFamily(),
		settings.readPref<QString>(kCjkFamilyKey),
		AyuSettings::getInstance().monoFont(),
	};
	for (const auto &entry : InstalledFonts()) {
		if (!ranges::any_of(entry.families, [&](const QString &family) {
			return active.contains(family);
		})) {
			continue;
		}
		for (const auto &path : entry.files) {
			const auto loaded = RegisterFont(Root() + path);
			if (loaded.empty()) {
				continue;
			}
		}
	}
	const auto family = Core::App().settings().readPref<QString>(kCjkFamilyKey);
	if (!family.isEmpty()) {
		for (const auto script : { QChar::Script_Han, QChar::Script_Hiragana, QChar::Script_Katakana }) {
			QFontDatabase::setApplicationFallbackFontFamilies(script, { family });
		}
	}
}

void SettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	controller->session().lifetime().add([weak = QPointer<Ui::GenericBox>(box)] {
		if (weak) {
			weak->closeBox();
		}
	});
	box->setTitle(tr::lng_regram_fonts());
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_regram_fonts_about(), st::boxLabel));
	const auto state = box->lifetime().make_state<BoxState>();
	box->addRow(object_ptr<Ui::FlatLabel>(box, state->status.value(), st::boxLabel));
	const auto search = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, tr::lng_regram_font_search()));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	state->refresh = [=] {
		rows->clear();
		const auto query = search->getLastText().trimmed();
		const auto installed = InstalledFonts();
		for (const auto &family : Catalog()) {
			const auto found = ranges::find(installed, family.id, &Installed::id);
			const auto available = (found != installed.end());
			const auto entry = available ? *found : Installed();
			const auto title = available ? entry.title : family.title;
			if (!title.contains(query, Qt::CaseInsensitive)
				&& !family.title.contains(query, Qt::CaseInsensitive)) {
				continue;
			}
			const auto button = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(title + (available ? QString() : u" ↓"_q)),
				st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				if (available) {
					box->uiShow()->showBox(Box(ManagedFontBox, controller, entry,
						crl::guard(box, [=] { state->refresh(); })));
				} else {
					Download(box, state, family);
				}
			});
		}
		for (const auto &entry : installed) {
			if (!entry.imported || !entry.title.contains(query, Qt::CaseInsensitive)) {
				continue;
			}
			const auto button = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(entry.title), st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				box->uiShow()->showBox(Box(ManagedFontBox, controller, entry,
					crl::guard(box, [=] { state->refresh(); })));
			});
		}
	};
	search->changes() | rpl::on_next([=] { state->refresh(); }, box->lifetime());
	state->refresh();
	const auto system = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_regram_font_system(), st::settingsButtonNoIcon));
	system->setClickedCallback([=] { box->uiShow()->showBox(Box(SystemFontsBox, controller)); });
	const auto clear = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_regram_font_clear_downloads(), st::settingsButtonNoIcon));
	clear->setClickedCallback([=] {
		box->uiShow()->showBox(Ui::MakeConfirmBox({
			.text = tr::lng_regram_font_clear_confirm(),
			.confirmed = crl::guard(box, [=](Fn<void()> close) {
				for (const auto &entry : InstalledFonts()) {
					if (!entry.imported && !RemoveFont(box, entry)) {
						return;
					}
				}
				state->refresh();
				close();
				Nagram::ShowRestartPrompt(controller);
			}),
		}));
	});
	box->addButton(tr::lng_regram_font_import(), [=] { ImportFont(box, state); });
	box->addButton(tr::lng_font_default(), [=] {
		box->uiShow()->showBox(Box(ChooseFamily, controller, QString()));
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Regram::Typography
