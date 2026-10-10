#include "regram/nagram/media/cover.h"

#include "regram/nagram/media/backend_options.h"
#include "data/data_document.h"
#include "ui/image/image_location.h"

namespace Nagram::Media {

DownloadLocation CoverLocation(
		not_null<DocumentData*> document,
		DownloadLocation upstream) {
	const auto song = document->song();
	const auto url = song
		? CoverRequestUrl(ForDevice(), song->performer, song->title)
		: QString();
	return url.isEmpty()
		? upstream
		: DownloadLocation{ PlainUrlLocation{ url } };
}

} // namespace Nagram::Media
