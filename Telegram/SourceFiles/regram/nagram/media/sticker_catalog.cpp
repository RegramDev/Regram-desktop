#include "regram/nagram/media/sticker_catalog.h"

#include "apiwrap.h"
#include "boxes/sticker_set_box.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers_set.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

constexpr auto kMaximumCatalogEntries = 1000;
constexpr auto kMaximumCatalogBytes = 1024 * 1024;
constexpr auto kTypes = std::array{
	Data::StickersType::Stickers,
	Data::StickersType::Masks,
	Data::StickersType::Emoji,
};

using Orders = std::array<Data::StickersSetsOrder, kTypes.size()>;

QString TypeName(Data::StickersType type) {
	return type == Data::StickersType::Emoji
		? u"emoji"_q
		: type == Data::StickersType::Masks
		? u"masks"_q
		: u"stickers"_q;
}

const Data::StickersSetsOrder &InstalledOrder(
		not_null<Main::Session*> session,
		Data::StickersType type) {
	const auto &stickers = session->data().stickers();
	return type == Data::StickersType::Emoji
		? stickers.emojiSetsOrder()
		: type == Data::StickersType::Masks
		? stickers.maskSetsOrder()
		: stickers.setsOrder();
}

bool ValidEntry(const StickerCatalogEntry &entry) {
	static const auto slug = QRegularExpression(u"\\A[a-zA-Z0-9_]{1,64}\\z"_q);
	return slug.match(entry.shortName).hasMatch()
		&& entry.title.size() <= 256
		&& QString::fromUtf8(entry.title.toUtf8()) == entry.title
		&& ranges::none_of(entry.title, [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		});
}

Orders CurrentOrders(not_null<Main::Session*> session) {
	auto result = Orders();
	for (auto i = 0; i != kTypes.size(); ++i) {
		result[i] = InstalledOrder(session, kTypes[i]);
	}
	return result;
}

bool ValidInstalledOrders(not_null<Main::Session*> session) {
	const auto &sets = session->data().stickers().sets();
	for (const auto &type : kTypes) {
		for (const auto &id : InstalledOrder(session, type)) {
			const auto i = sets.find(id);
			if (i == sets.end()
				|| !(i->second->flags & Data::StickersSetFlag::Installed)
				|| (i->second->flags & Data::StickersSetFlag::Archived)) {
				return false;
			}
		}
	}
	return true;
}

void ApplyCatalogOrder(
		not_null<Window::SessionController*> controller,
		const StickerCatalog &catalog,
		const Orders &before) {
	const auto session = &controller->session();
	if (CurrentOrders(session) != before) {
		controller->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return;
	} else if (!ValidInstalledOrders(session)) {
		controller->showToast(tr::lng_nagram_catalog_incomplete(tr::now));
		return;
	}
	auto planned = Orders();
	auto count = 0;
	for (auto i = 0; i != kTypes.size(); ++i) {
		planned[i] = CatalogStickerOrder(session, catalog, kTypes[i]);
		count += (planned[i] != before[i]);
	}
	if (!count) {
		controller->showToast(tr::lng_nagram_catalog_unchanged(tr::now));
		return;
	}
	for (auto i = 0; i != kTypes.size(); ++i) {
		if (planned[i] == before[i]) {
			continue;
		}
		session->api().saveStickerSets(
			planned[i],
			{},
			kTypes[i]);
	}
	controller->showToast(tr::lng_nagram_catalog_submitted(tr::now));
}

void ImportCatalog(
		not_null<Window::SessionController*> controller,
		QByteArray bytes) {
	if (const auto catalog = ParseStickerCatalog(bytes)) {
		PreviewStickerCatalog(controller, *catalog);
	} else {
		controller->showToast(tr::lng_nagram_catalog_invalid(tr::now));
	}
}

} // namespace

std::optional<StickerCatalog> ParseStickerCatalog(const QByteArray &bytes) {
	if (bytes.size() > kMaximumCatalogBytes) {
		return std::nullopt;
	}
	const auto document = QJsonDocument::fromJson(bytes);
	const auto object = document.object();
	const auto sets = object.value(u"sets"_q);
	if (!document.isObject() || object.size() != 3
		|| object.value(u"format"_q) != u"nagram-sticker-catalog"_q
		|| object.value(u"version"_q) != QJsonValue(1)
		|| !sets.isArray() || sets.toArray().size() > kMaximumCatalogEntries) {
		return std::nullopt;
	}
	auto result = StickerCatalog();
	auto seen = QSet<QString>();
	for (const auto &value : sets.toArray()) {
		const auto fields = value.toObject();
		const auto type = fields.value(u"type"_q).toString();
		if (!value.isObject() || fields.size() != 3
			|| !fields.value(u"shortName"_q).isString()
			|| !fields.value(u"title"_q).isString()
			|| (type != u"stickers"_q && type != u"masks"_q && type != u"emoji"_q)) {
			return std::nullopt;
		}
		auto entry = StickerCatalogEntry{
			.shortName = fields.value(u"shortName"_q).toString(),
			.title = fields.value(u"title"_q).toString(),
			.type = type == u"emoji"_q
				? Data::StickersType::Emoji
				: type == u"masks"_q
				? Data::StickersType::Masks
				: Data::StickersType::Stickers,
		};
		const auto key = entry.shortName.toLower();
		if (!ValidEntry(entry) || seen.contains(key)) {
			return std::nullopt;
		}
		seen.insert(key);
		result.push_back(std::move(entry));
	}
	return result;
}

QByteArray SerializeStickerCatalog(const StickerCatalog &catalog) {
	auto sets = QJsonArray();
	for (const auto &entry : catalog) {
		sets.push_back(QJsonObject{
			{ u"shortName"_q, entry.shortName },
			{ u"title"_q, entry.title },
			{ u"type"_q, TypeName(entry.type) },
		});
	}
	return QJsonDocument(QJsonObject{
		{ u"format"_q, u"nagram-sticker-catalog"_q },
		{ u"version"_q, 1 },
		{ u"sets"_q, sets },
	}).toJson(QJsonDocument::Indented);
}

std::variant<StickerCatalog, QString> CurrentStickerCatalog(
		not_null<Main::Session*> session) {
	auto result = StickerCatalog();
	const auto &sets = session->data().stickers().sets();
	if (!ValidInstalledOrders(session)) {
		return tr::lng_nagram_catalog_incomplete(tr::now);
	}
	for (const auto &type : kTypes) {
		for (const auto &id : InstalledOrder(session, type)) {
			const auto i = sets.find(id);
			if (i == sets.end()) {
				return tr::lng_nagram_catalog_incomplete(tr::now);
			}
			const auto &set = i->second;
			if (set->shortName.isEmpty()) {
				continue;
			}
			result.push_back({ set->shortName, set->title, type });
		}
	}
	if (!ParseStickerCatalog(SerializeStickerCatalog(result))) {
		return tr::lng_nagram_catalog_invalid(tr::now);
	}
	return result;
}

Data::StickersSetsOrder CatalogStickerOrder(
		not_null<Main::Session*> session,
		const StickerCatalog &catalog,
		Data::StickersType type) {
	const auto &original = InstalledOrder(session, type);
	const auto &sets = session->data().stickers().sets();
	auto byName = QHash<QString, uint64>();
	for (const auto &id : original) {
		const auto i = sets.find(id);
		if (i != sets.end() && !i->second->shortName.isEmpty()) {
			byName.insert(i->second->shortName.toLower(), id);
		}
	}
	auto result = Data::StickersSetsOrder();
	for (const auto &entry : catalog) {
		if (entry.type == type) {
			const auto id = byName.value(entry.shortName.toLower());
			if (id && !result.contains(id)) {
				result.push_back(id);
			}
		}
	}
	for (const auto &id : original) {
		if (!result.contains(id)) {
			result.push_back(id);
		}
	}
	return result;
}

void PreviewStickerCatalog(
		not_null<Window::SessionController*> controller,
		const StickerCatalog &catalog) {
	if (!ParseStickerCatalog(SerializeStickerCatalog(catalog))) {
		controller->showToast(tr::lng_nagram_catalog_invalid(tr::now));
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_catalog_preview());

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_catalog_preview_about(),
			st::boxLabel));
		const auto session = &controller->session();
		const auto before = box->lifetime().make_state<Orders>();
		const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
		const auto refresh = [=] {
			*before = CurrentOrders(session);
			rows->clear();
			const auto current = CurrentStickerCatalog(session);
			const auto installed = std::get_if<StickerCatalog>(&current);
			if (!installed) {
				rows->add(object_ptr<Ui::FlatLabel>(
					rows,
					std::get<QString>(current),
					st::boxLabel));
			}
			for (const auto &entry : catalog) {
				const auto found = installed && ranges::any_of(
					*installed,
					[&](const StickerCatalogEntry &other) {
						return other.type == entry.type
							&& other.shortName.compare(
								entry.shortName,
								Qt::CaseInsensitive) == 0;
					});
				const auto label = (entry.title.isEmpty()
					? entry.shortName
					: entry.title) + u" · "_q + (found
					? tr::lng_nagram_catalog_installed(tr::now)
					: tr::lng_nagram_catalog_open(tr::now));
				const auto button = rows->add(object_ptr<Ui::SettingsButton>(
					rows,
					rpl::single(label),
					st::settingsButtonNoIcon));
				button->setClickedCallback([=] {
					controller->show(Box<StickerSetBox>(
						controller->uiShow(),
						StickerSetIdentifier{ .shortName = entry.shortName },
						entry.type), Ui::LayerOption::KeepOther);
				});
			}
		};
		session->data().stickers().updated() | rpl::on_next([=] {
			InvokeQueued(box, refresh);
		}, box->lifetime());
		box->addButton(tr::lng_nagram_catalog_apply_order(), [=] {
			ApplyCatalogOrder(controller, catalog, *before);
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		refresh();
	}), Ui::LayerOption::KeepOther);
}

void ShowStickerCatalog(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_catalog_title());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_catalog_about(),
			st::boxLabel));
		box->addButton(tr::lng_nagram_config_export(), [=] {
			const auto current = CurrentStickerCatalog(&controller->session());
			if (const auto error = std::get_if<QString>(&current)) {
				box->showToast(*error);
				return;
			}
			const auto bytes = SerializeStickerCatalog(
				std::get<StickerCatalog>(current));
			FileDialog::GetWritePath(
				Core::App().getFileDialogParent(),
				tr::lng_nagram_config_export(tr::now),
				tr::lng_nagram_config_file_filter(tr::now),
				u"regram-stickers.json"_q,
				crl::guard(box, [=](QString &&path) {
					if (path.isEmpty()) {
						return;
					}
					auto file = QSaveFile(path);
					if (!file.open(QIODevice::WriteOnly)
						|| file.write(bytes) != bytes.size()
						|| !file.commit()) {
						box->showToast(tr::lng_nagram_config_write_error(tr::now));
					} else {
						box->showToast(tr::lng_nagram_config_exported(tr::now));
					}
				}));
		});
		box->addButton(tr::lng_nagram_config_import(), [=] {
			FileDialog::GetOpenPath(
				Core::App().getFileDialogParent(),
				tr::lng_nagram_config_import(tr::now),
				tr::lng_nagram_config_file_filter(tr::now),
				crl::guard(controller, [=](FileDialog::OpenResult &&result) {
					if (!result.paths.isEmpty()) {
						auto file = QFile(result.paths.front());
						if (!file.open(QIODevice::ReadOnly)) {
							controller->showToast(
								tr::lng_nagram_config_read_error(tr::now));
							return;
						}
						ImportCatalog(controller, file.read(kMaximumCatalogBytes + 1));
					} else if (!result.remoteContent.isEmpty()) {
						ImportCatalog(controller, result.remoteContent);
					}
				}));
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram
