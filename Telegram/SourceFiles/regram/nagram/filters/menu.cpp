#include "regram/nagram/filters/menu.h"

#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/filters/reveal.h"
#include "regram/filters/runtime.h"
#include "regram/nagram/menu/actions.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"

namespace Nagram::Filters {
namespace {

void ToggleSender(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	if (!Regram::Filters::SetSenderHidden(peer, !Regram::Filters::SenderHidden(peer))) {
		controller->showToast(tr::lng_regram_filter_scope_limit(tr::now));
	}
}

} // namespace

void InsertAuthorAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !item || !controller || (item->from()->isSelf() || !item->from()->isUser())) {
		return;
	}
	const auto author = item->from();
	const auto hidden = Regram::Filters::SenderHidden(author);
	const auto title = hidden
		? tr::lng_nagram_filter_author_show(tr::now)
		: tr::lng_nagram_filter_author_hide(tr::now);
	const auto action = Ui::Menu::CreateAction(menu, title,
		crl::guard(controller, [=] { ToggleSender(controller, author); }));
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
		Data::ForumTopic *) {
	if (!peer) {
		return;
	}
	const auto session = &controller->session();
	if (!peer->isSelf() && peer->isUser()) {
		const auto hidden = Regram::Filters::SenderHidden(peer);
		addAction(hidden ? tr::lng_nagram_filter_author_show(tr::now)
			: tr::lng_nagram_filter_author_hide(tr::now),
			crl::guard(controller, [=] { ToggleSender(controller, peer); }),
			&st::menuIconBlock);
	}
	const auto hasRules = !ForAccount(session).Get(Regram::Filters::kPortableRules).isEmpty();
	const auto enabled = Regram::Filters::EnabledForPeer(peer);
	if (hasRules || !enabled) {
		addAction(enabled
			? tr::lng_regram_filter_disable_chat(tr::now)
			: tr::lng_regram_filter_enable_chat(tr::now),
			crl::guard(controller, [=] {
				if (!Regram::Filters::SetEnabledForPeer(peer, !enabled)) {
					controller->showToast(tr::lng_nagram_filter_invalid(tr::now));
				}
			}), &st::menuIconTagFilter);
	}
	const auto revealed = Regram::Filters::TemporarilyRevealed(peer);
	if (hasRules || revealed || !Regram::Filters::HiddenSenders(session).empty()) {
		addAction(revealed ? tr::lng_regram_filter_reveal_end(tr::now)
			: tr::lng_regram_filter_reveal(tr::now), crl::guard(controller, [=] {
			if (!Regram::Filters::ToggleTemporaryReveal(peer)) {
				controller->showToast(tr::lng_regram_filter_scope_limit(tr::now));
			}
		}), revealed ? &st::menuIconCaptionHide : &st::menuIconCaptionShow);
	}
}

} // namespace Nagram::Filters
