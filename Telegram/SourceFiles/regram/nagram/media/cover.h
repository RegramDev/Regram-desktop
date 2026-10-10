#pragma once

class DocumentData;
class DownloadLocation;

namespace Nagram::Media {

[[nodiscard]] DownloadLocation CoverLocation(
	not_null<DocumentData*> document,
	DownloadLocation upstream);

} // namespace Nagram::Media
