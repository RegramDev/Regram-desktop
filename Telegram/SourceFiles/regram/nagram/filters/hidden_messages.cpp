#include "regram/nagram/filters/hidden_messages.h"

#include "regram/nagram/filters/model.h"
#include "regram/nagram/menu/actions.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QSet>

namespace Nagram::Filters {
namespace {

QString Key(not_null<HistoryItem*> item) {
	return QString::number(item->history()->peer->id.value)
		+ u':'
		+ QString::number(item->id.bare);
}

QStringList Read(not_null<Main::Session*> session) {
	return ForAccount(session).Get(kHiddenMessages).split(
		u',',
		Qt::SkipEmptyParts);
}

bool Write(not_null<Main::Session*> session, const QStringList &keys) {
	return ForAccount(session).Set(kHiddenMessages, keys.join(u','));
}

} // namespace

bool MessageHidden(not_null<HistoryItem*> item) {
	if (!item->isRegular()) {
		return false;
	}
	const auto raw = ForAccount(&item->history()->session()).Get(
		kHiddenMessages);
	if (raw.isEmpty()) {
		return false;
	}
	static auto previous = QString();
	static auto keys = QSet<QString>();
	if (previous != raw) {
		const auto list = raw.split(u',', Qt::SkipEmptyParts);
		keys = QSet<QString>(list.begin(), list.end());
		previous = raw;
	}
	return keys.contains(Key(item));
}

int HiddenMessagesCount(not_null<Main::Session*> session) {
	return Read(session).size();
}

void ClearHiddenMessages(not_null<Main::Session*> session) {
	Expects(Write(session, {}));
}

void InsertHideMessageAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !item || !controller || !item->isRegular()
		|| item->isService()) {
		return;
	}
	const auto session = &controller->session();
	const auto itemId = item->fullId();
	const auto hidden = MessageHidden(item);
	const auto action = Ui::Menu::CreateAction(menu,
		(hidden
			? tr::lng_nagram_hide_message_undo
			: tr::lng_nagram_hide_message)(tr::now),
		crl::guard(controller, [=] {
			const auto current = session->data().message(itemId);
			if (!current) {
				return;
			}
			auto keys = Read(session);
			const auto key = Key(current);
			if (!keys.removeAll(key)) {
				if (keys.size() >= kMaximumHiddenMessages) {
					controller->showToast(
						tr::lng_nagram_hide_message_limit(tr::now));
					return;
				}
				keys.push_back(key);
			}
			if (!Write(session, keys)) {
				controller->showToast(tr::lng_nagram_filter_invalid(tr::now));
			}
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconStealth, &st::menuIconStealth);
	Menu::Tag(menu->insertAction(Menu::EndPosition(menu), std::move(widget)),
		Menu::ActionId::HideMessage);
}

} // namespace Nagram::Filters
