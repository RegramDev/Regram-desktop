#include "regram/nagram/menu/media.h"

#include "regram/nagram/menu/actions.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Nagram::Menu {
namespace {

QString Details(HistoryItem *item) {
	const auto media = item ? item->media() : nullptr;
	if (!media) {
		return {};
	}
	auto lines = QStringList();
	const auto add = [&](QString name, QString value) {
		if (!value.isEmpty()) {
			lines.push_back(name + u": "_q + value);
		}
	};
	auto dimensions = QSize();
	auto bytes = int64(0);
	if (const auto document = media->document()) {
		add(tr::lng_nagram_menu_media_filename(tr::now), document->filename());
		add(tr::lng_nagram_menu_media_type(tr::now), document->mimeString());
		dimensions = document->dimensions;
		bytes = document->size;
		if (document->hasDuration()) {
			add(tr::lng_nagram_menu_media_duration(tr::now),
				QString::number(document->duration()) + u" ms"_q);
		}
	} else if (const auto photo = media->photo()) {
		dimensions = photo->size(Data::PhotoSize::Large).value_or(QSize());
		bytes = photo->imageByteSize(Data::PhotoSize::Large);
	} else {
		return {};
	}
	if (!dimensions.isEmpty()) {
		add(tr::lng_nagram_menu_media_dimensions(tr::now),
			QString::number(dimensions.width()) + u" × "_q
				+ QString::number(dimensions.height()) + u" px"_q);
	}
	if (bytes > 0) {
		add(tr::lng_nagram_menu_media_bytes(tr::now), QString::number(bytes));
	}
	return lines.isEmpty()
		? tr::lng_nagram_menu_media_unknown(tr::now)
		: lines.join('\n');
}

} // namespace

void InsertMediaInfoAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !item || !controller || Details(item).isEmpty()) {
		return;
	}
	const auto itemId = item->fullId();
	menu = Destination(menu, ActionId::MediaInfo);
	const auto action = Ui::Menu::CreateAction(menu,
		tr::lng_nagram_menu_media_info(tr::now),
		crl::guard(controller, [=] {
			const auto current = controller->session().data().message(itemId);
			const auto text = Details(current);
			if (text.isEmpty()) {
				return;
			}
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_nagram_menu_media_info());
				const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
					box, text, st::boxLabel));
				label->setSelectable(true);
				box->addButton(tr::lng_close(), [=] { box->closeBox(); });
			}));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconInfo, &st::menuIconInfo);
	Tag(menu->insertAction(EndPosition(menu), std::move(widget)),
		ActionId::MediaInfo);
}

} // namespace Nagram::Menu
