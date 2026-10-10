#include "regram/nagram/filters/menu.h"

#include "regram/nagram/filters/model.h"
#include "regram/nagram/filters/settings.h"
#include "regram/nagram/menu/actions.h"
#include "data/data_forum_topic.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Nagram::Filters {

void InsertAuthorAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !item || !controller || !item->from()) {
		return;
	}
	const auto session = &controller->session();
	const auto author = QString::number(SerializePeerId(item->from()->id));
	const auto raw = ForAccount(session).Get(kRules);
	if (!Validate(raw)) {
		return;
	}
	const auto config = raw.isEmpty()
		? Defaults() : QJsonDocument::fromJson(raw).object();
	const auto hidden = config.value(u"hiddenAuthors"_q).toArray().contains(author);
	const auto title = hidden
		? tr::lng_nagram_filter_author_show(tr::now)
		: tr::lng_nagram_filter_author_hide(tr::now);
	const auto action = Ui::Menu::CreateAction(menu, title,
		crl::guard(controller, [=] {
			const auto current = ForAccount(session).Get(kRules);
			if (!Validate(current)) {
				controller->showToast(tr::lng_nagram_filter_invalid(tr::now));
				return;
			}
			auto updated = current.isEmpty()
				? Defaults() : QJsonDocument::fromJson(current).object();
			auto authors = updated.value(u"hiddenAuthors"_q).toArray();
			if (const auto index = authors.toVariantList().indexOf(author); index >= 0) {
				authors.removeAt(index);
			} else {
				authors.push_back(author);
				updated.insert(u"enabled"_q, true);
			}
			updated.insert(u"hiddenAuthors"_q, authors);
			const auto bytes = QJsonDocument(updated).toJson(QJsonDocument::Compact);
			if (!ForAccount(session).Set(kRules, bytes)) {
				controller->showToast(tr::lng_nagram_filter_invalid(tr::now));
				return;
			}
			controller->showToast(tr::lng_nagram_filter_menu_saved(tr::now));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action,
		&st::menuIconBlock, &st::menuIconBlock);
	Menu::Tag(menu->insertAction(Menu::EndPosition(menu), std::move(widget)),
		Menu::ActionId::FilterAuthor);
}

void AddScopeAction(
		const Ui::Menu::MenuCallback &addAction,
		not_null<Window::SessionController*> controller,
		PeerData *peer,
		Data::ForumTopic *topic) {
	if (!peer) {
		return;
	}
	const auto session = &controller->session();
	const auto account = ForAccount(session).Get(kRules);
	const auto scopes = ForAccount(session).Get(kScopes);
	const auto peerId = QString::number(SerializePeerId(peer->id));
	const auto topicId = QString::number(topic ? topic->rootId().bare : 0);
	const auto own = [&] {
		for (const auto &entry : QJsonDocument::fromJson(scopes).object(
				).value(u"scopes"_q).toArray()) {
			const auto scope = entry.toObject();
			if (scope.value(u"peer"_q) == peerId
				&& scope.value(u"topic"_q) == topicId) {
				return true;
			}
		}
		return false;
	}();
	if (!own
		&& ForDevice().Get(kGlobalRules).isEmpty()
		&& !QJsonDocument::fromJson(account).object().value(
			u"enabled"_q).toBool()) {
		return;
	}
	addAction(topic
		? tr::lng_nagram_filter_scope_topic(tr::now)
		: tr::lng_nagram_filter_scope_chat(tr::now),
		crl::guard(controller, [=] {
			controller->show(
				Box(ScopeSettingsBox, session, peerId, topicId));
		}),
		&st::menuIconBlock);
}

} // namespace Nagram::Filters
