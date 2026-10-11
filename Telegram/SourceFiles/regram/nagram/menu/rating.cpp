#include "regram/nagram/menu/rating.h"

#include "regram/nagram/menu/actions.h"
#include "regram/nagram/menu/draft.h"
#include "data/data_chat_participant_status.h"
#include "data/data_forum_topic.h"
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

namespace Nagram::Menu {
namespace {

constexpr auto kActionIdProperty = "nagramMenuActionId";

Data::Thread *Target(HistoryItem *item) {
	if (!item || item->isService() || item->isLocal()
		|| !item->isRegular()) {
		return nullptr;
	}
	const auto target = item->topic()
		? static_cast<Data::Thread*>(item->topic())
		: static_cast<Data::Thread*>(item->history());
	return Data::CanSendTexts(target) ? target : nullptr;
}

int InsertPosition(not_null<Ui::PopupMenu*> menu) {
	for (auto index = 0; index != int(menu->actions().size()); ++index) {
		const auto value = menu->actions()[index]->property(kActionIdProperty);
		if (value.isValid()
			&& value.toInt() == static_cast<int>(ActionId::Reply)) {
			return index + 1;
		}
	}
	return 0;
}

void FillDraft(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId,
		const QString &text) {
	const auto item = controller->session().data().message(itemId);
	const auto target = Target(item);
	if (!target) {
		controller->showToast(tr::lng_nagram_menu_batch_unavailable(tr::now));
		return;
	}
	if (DraftOccupied(target)) {
		controller->showToast(
			tr::lng_nagram_menu_batch_draft_exists(tr::now));
		return;
	}
	PlaceTextDraft(controller, target, { text }, itemId);
}

} // namespace

void InsertQuickRatingActions(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	if (!menu || !controller || !Target(item)) {
		return;
	}
	const auto itemId = item->fullId();
	menu = Destination(menu, ActionId::QuickRating);
	auto position = InsertPosition(menu);
	for (const auto &option : { &kQuickRatingFirst, &kQuickRatingSecond }) {
		const auto text = ForDevice().Get(*option).trimmed();
		if (text.isEmpty()) {
			continue;
		}
		const auto action = Ui::Menu::CreateAction(menu,
			tr::lng_nagram_menu_quick_rating_item(tr::now, lt_text, text),
			crl::guard(controller, [=] {
				FillDraft(controller, itemId, text);
			}));
		auto widget = base::make_unique_q<Ui::Menu::Action>(
			menu->menu(), menu->menu()->st(), action,
			&st::menuIconReply, &st::menuIconReply);
		Tag(menu->insertAction(position++, std::move(widget)),
			ActionId::QuickRating);
	}
}

} // namespace Nagram::Menu
