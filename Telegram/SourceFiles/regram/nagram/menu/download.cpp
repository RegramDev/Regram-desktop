#include "regram/nagram/menu/download.h"

#include "regram/nagram/menu/actions.h"
#include "core/application.h"
#include "data/data_document.h"
#include "data/data_download_manager.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Nagram::Menu {
namespace {

DocumentData *DownloadedDocument(HistoryItem *item) {
	const auto media = item ? item->media() : nullptr;
	const auto document = media ? media->document() : nullptr;
	if (!document || document->loading() || document->sticker()
		|| document->filepath(true).isEmpty()) {
		return nullptr;
	}
	return document;
}

void DeleteDownloaded(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId,
		const QString &path) {
	const auto item = controller->session().data().message(itemId);
	const auto document = DownloadedDocument(item);
	if (!document || document->filepath(true) != path) {
		controller->showToast(
			tr::lng_nagram_menu_delete_download_changed(tr::now));
		return;
	}
	if (!QFile::moveToTrash(path)) {
		controller->showToast(
			tr::lng_nagram_menu_delete_download_failed(tr::now));
		return;
	}
	Core::App().downloadManager().deleteFiles({ item->globalId() });
	[[maybe_unused]] const auto location = document->location(true);
	controller->session().data().requestItemRepaint(item);
	controller->showToast(tr::lng_nagram_menu_delete_download_done(tr::now));
}

} // namespace

void InsertDeleteDownloadAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	const auto document = DownloadedDocument(item);
	if (!menu || !controller || !document) {
		return;
	}
	const auto itemId = item->fullId();
	const auto path = document->filepath(true);
	const auto weak = base::make_weak(controller);
	const auto action = Ui::Menu::CreateAction(menu,
		tr::lng_nagram_menu_delete_download(tr::now),
		crl::guard(controller, [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_menu_delete_download_confirm(
					tr::now,
					lt_file,
					QFileInfo(path).fileName()),
				.confirmed = [=](Fn<void()> &&close) {
					close();
					if (const auto strong = weak.get()) {
						DeleteDownloaded(strong, itemId, path);
					}
				},
				.confirmText = tr::lng_box_delete(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconClear, &st::menuIconClear);
	Tag(menu->insertAction(EndPosition(menu), std::move(widget)),
		ActionId::DeleteDownload);
}

} // namespace Nagram::Menu
