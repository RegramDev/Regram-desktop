#pragma once

#include "data/stickers/data_stickers.h"

namespace Window { class SessionController; }

namespace Nagram {

struct StickerCatalogEntry {
	QString shortName;
	QString title;
	Data::StickersType type = Data::StickersType::Stickers;

	friend bool operator==(const StickerCatalogEntry&, const StickerCatalogEntry&)
		= default;
};

using StickerCatalog = std::vector<StickerCatalogEntry>;

[[nodiscard]] std::optional<StickerCatalog> ParseStickerCatalog(
	const QByteArray &bytes);
[[nodiscard]] QByteArray SerializeStickerCatalog(const StickerCatalog &catalog);
[[nodiscard]] std::variant<StickerCatalog, QString> CurrentStickerCatalog(
	not_null<Main::Session*> session);
[[nodiscard]] Data::StickersSetsOrder CatalogStickerOrder(
	not_null<Main::Session*> session,
	const StickerCatalog &catalog,
	Data::StickersType type);
void ShowStickerCatalog(not_null<Window::SessionController*> controller);
void PreviewStickerCatalog(
	not_null<Window::SessionController*> controller,
	const StickerCatalog &catalog);

} // namespace Nagram
