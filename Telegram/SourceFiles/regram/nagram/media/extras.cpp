#include "regram/nagram/media/extras.h"

#include "data/data_document.h"
#include "info/downloads/info_downloads_widget.h"
#include "info/info_memento.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"

#include <QtCore/QFileInfo>

namespace Nagram::Media {
namespace {

constexpr auto kExecutableSuffixes = std::array{
	"exe", "msi", "com", "scr", "bat", "cmd", "ps1", "vbs", "jar",
	"apk", "dmg", "pkg", "app", "sh", "run", "appimage", "deb", "rpm",
};
constexpr auto kArchiveSuffixes = std::array{
	"zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "xz", "zst", "lz", "cab",
};

template <std::size_t Size>
bool HasSuffix(
		const QString &suffix,
		const std::array<const char*, Size> &list) {
	for (const auto &entry : list) {
		if (suffix == QLatin1String(entry)) {
			return true;
		}
	}
	return false;
}

} // namespace

int GifMaxSize(int fallback) {
	return ForDevice().Get(kSmallGifs) ? (fallback * 2 / 3) : fallback;
}

int StickerPanelCell(int fallback) {
	return fallback * ForDevice().Get(kStickerPanelScale) / 100;
}

bool AutoDownloadBlocked(not_null<DocumentData*> document) {
	const auto executables = ForDevice().Get(kBlockExecutableAutoDownload);
	const auto archives = ForDevice().Get(kBlockArchiveAutoDownload);
	if (!executables && !archives) {
		return false;
	}
	const auto suffix = QFileInfo(document->filename()).suffix().toLower();
	return (executables && HasSuffix(suffix, kExecutableSuffixes))
		|| (archives && HasSuffix(suffix, kArchiveSuffixes));
}

void ShowDownloads(not_null<Window::SessionController*> controller) {
	controller->showSection(
		Info::Downloads::Make(controller->session().user()));
}

} // namespace Nagram::Media
