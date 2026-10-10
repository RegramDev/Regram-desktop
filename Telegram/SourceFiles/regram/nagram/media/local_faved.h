#pragma once

#include "data/stickers/data_stickers_set.h"

class DocumentData;

namespace Main {
class Session;
} // namespace Main

namespace Nagram::Media {

[[nodiscard]] bool KeepOverflowFaved(
	not_null<Main::Session*> session,
	not_null<DocumentData*> document);
[[nodiscard]] bool LocalFaved(not_null<const DocumentData*> document);
void RemoveLocalFaved(not_null<DocumentData*> document);
[[nodiscard]] Data::StickersPack WithLocalFaved(
	not_null<Main::Session*> session,
	Data::StickersPack pack);

[[nodiscard]] rpl::producer<int> LocalFavedCountValue(
	not_null<Main::Session*> session);
void ClearLocalFaved(not_null<Main::Session*> session);

} // namespace Nagram::Media
