#include "regram/nagram/chats/layout.h"

#include "regram/nagram/chats/options.h"
#include "lang/lang_keys.h"
#include "styles/style_dialogs.h"

#include <algorithm>

namespace Nagram::Chats {

const style::DialogRow &RowStyle(bool hasTags, bool wideRow) {
	if (wideRow) {
		return hasTags ? st::taggedForumDialogRow : st::forumDialogRow;
	} else if (hasTags) {
		return st::taggedDialogRow;
	}
	const auto compact = ForDevice().Get(kCompactList);
	switch (ForDevice().Get(kPreviewLines)) {
	case 2: return compact ? st::compactTwoLineDialogRow : st::twoLineDialogRow;
	case 3: return compact ? st::compactThreeLineDialogRow : st::threeLineDialogRow;
	default: return compact ? st::compactDialogRow : st::defaultDialogRow;
	}
}

int PreviewLines() {
	return std::max(ForDevice().Get(kPreviewLines), 1);
}

bool HideSavedAndArchivedPreviews() {
	return ForDevice().Get(kHideSavedAndArchivedPreviews);
}

bool HidePreview(bool folder, bool savedMessages) {
	return (folder || savedMessages) && HideSavedAndArchivedPreviews();
}

QString HiddenPreviewText(bool folder) {
	return folder
		? tr::lng_nagram_archive_preview_placeholder(tr::now)
		: tr::lng_nagram_saved_preview_placeholder(tr::now);
}

bool HideStories() {
	return ForDevice().Get(kHideStories);
}

bool ShowArchiveInFolders() {
	return ForDevice().Get(kShowArchiveInFolders);
}

bool HideFolderUnreadCounters() {
	return ForDevice().Get(kHideFolderUnreadCounters);
}

} // namespace Nagram::Chats
