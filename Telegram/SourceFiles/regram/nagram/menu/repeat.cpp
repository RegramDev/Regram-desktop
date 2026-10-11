#include "regram/nagram/menu/repeat.h"

#include "regram/nagram/menu/actions.h"
#include "regram/nagram/privacy/protection.h"
#include "api/api_common.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "data/data_chat_participant_status.h"
#include "data/data_forum_topic.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

namespace Nagram::Menu {
namespace {

constexpr auto kActionIdProperty = "nagramMenuActionId";

bool Sendable(HistoryItem *item) {
	return item && !item->isService() && !item->isLocal() && item->id > 0;
}

bool CanForward(HistoryItem *item) {
	return Sendable(item) && item->allowsForward();
}

bool HasCopy(not_null<HistoryItem*> item) {
	const auto media = item->media();
	return (media && !media->webpage())
		? (media->photo() || media->document())
		: !item->originalText().text.isEmpty();
}

// WHY: Telegram rejects forwards out of a protected chat, so with the
// copy bypass on the same actions resend the content as new messages.
bool CopyOnly(HistoryItem *item) {
	return Sendable(item)
		&& item->isRegular()
		&& !item->allowsForward()
		&& item->allowsMediaDownloadControls()
		&& HasCopy(item);
}

not_null<Data::Thread*> RepeatTarget(not_null<HistoryItem*> item) {
	return item->topic()
		? static_cast<Data::Thread*>(item->topic())
		: static_cast<Data::Thread*>(item->history());
}

// WHY: comments are plain messages of the discussion group, so only
// the opened section tells that a repeat belongs to their thread.
MsgId OpenedThreadRootId(
		not_null<Window::SessionController*> controller,
		not_null<Data::Thread*> target) {
	const auto state = controller->dialogsEntryStateCurrent();
	return ((state.section == Dialogs::EntryState::Section::Replies)
		&& (state.key.history() == target.get()))
		? state.currentReplyTo.topicRootId
		: MsgId();
}

bool Available(
		not_null<Window::SessionController*> controller,
		HistoryItem *item) {
	if (!CanForward(item) && !CopyOnly(item)) {
		return false;
	}
	const auto target = RepeatTarget(item);
	return Data::CanSendAnything(target)
		&& (HasCopy(item) || !OpenedThreadRootId(controller, target));
}

Api::SendAction RepeatAction(not_null<Data::Thread*> target, MsgId rootId) {
	auto result = Api::SendAction(target);
	result.clearDraft = false;
	if (rootId) {
		result.replyTo = {
			.messageId = { result.history->peer->id, rootId },
			.topicRootId = rootId,
		};
	}
	return result;
}

void ShowLatest(
		not_null<Window::SessionController*> controller,
		not_null<Data::Thread*> target,
		MsgId rootId) {
	if (!ForDevice().Get(kScrollAfterRepeat)) {
		return;
	} else if (rootId) {
		controller->showRepliesForMessage(
			target->owningHistory(),
			rootId,
			ShowAtTheEndMsgId);
	} else {
		controller->showThread(target, ShowAtTheEndMsgId);
	}
}

bool SendCopy(not_null<HistoryItem*> item, Api::SendAction action) {
	if (!HasCopy(item)) {
		return false;
	}
	const auto media = item->media();
	const auto plain = !media || media->webpage();
	const auto &original = item->originalText();
	auto message = Api::MessageToSend(std::move(action));
	message.textWithTags = { original.text,
		TextUtilities::ConvertEntitiesToTextTags(original.entities) };
	if (plain) {
		item->history()->session().api().sendMessage(std::move(message));
	} else if (const auto photo = media->photo()) {
		Api::SendExistingPhoto(std::move(message), photo);
	} else {
		Api::SendExistingDocument(std::move(message), media->document());
	}
	return true;
}

int SendCopies(not_null<HistoryItem*> item, const Api::SendAction &action) {
	const auto owner = &item->history()->owner();
	auto sent = 0;
	for (const auto &id : owner->itemOrItsGroup(item)) {
		const auto part = owner->message(id);
		sent += (part && SendCopy(part, action)) ? 1 : 0;
	}
	return sent;
}

void SendRepeat(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId,
		ActionId id) {
	const auto item = controller->session().data().message(itemId);
	if (!Available(controller, item)) {
		return;
	}
	const auto target = RepeatTarget(item);
	const auto history = item->history();
	const auto rootId = OpenedThreadRootId(controller, target);
	// A forward cannot reply to the post, so a comment thread gets a copy.
	if (CopyOnly(item)
		|| rootId
		|| (id == ActionId::Repeat && ForDevice().Get(kRepeatWithoutQuote))) {
		id = ActionId::RepeatAsCopy;
	}
	if (id == ActionId::RepeatAsCopy && (CopyOnly(item) || rootId)) {
		if (SendCopies(item, RepeatAction(target, rootId))) {
			ShowLatest(controller, target, rootId);
		}
		return;
	}
	auto draft = Data::ForwardDraft{
		.ids = history->owner().itemOrItsGroup(item),
		.options = (id == ActionId::Repeat)
			? Data::ForwardOptions::PreserveInfo
			: Data::ForwardOptions::NoSenderNames,
	};
	auto resolved = history->resolveForwardDraft(draft);
	if (resolved.items.empty()) {
		return;
	}
	history->session().api().forwardMessages(
		std::move(resolved),
		RepeatAction(target, rootId));
	ShowLatest(controller, target, rootId);
}

void ShowForwardCopy(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	const auto weak = base::make_weak(controller);
	const auto chooser = std::make_shared<base::weak_qptr<Ui::BoxContent>>();
	const auto send = [=](base::weak_ptr<Data::Thread> thread) {
		const auto strong = weak.get();
		const auto target = thread.get();
		if (!strong || !target) {
			return;
		}
		const auto item = strong->session().data().message(itemId);
		if (!CopyOnly(item)
			|| !SendCopies(item, RepeatAction(target, MsgId()))) {
			strong->showToast(tr::lng_forward_cant(tr::now));
			return;
		}
		if (const auto box = chooser->get()) {
			box->closeBox();
		}
		strong->showToast(tr::lng_share_done(tr::now));
	};
	auto chosen = [=](not_null<Data::Thread*> thread) {
		if (!Data::CanSendAnything(thread)
			|| thread->peer()->starsPerMessageChecked()) {
			controller->show(Ui::MakeInformBox(tr::lng_forward_cant()));
			return false;
		}
		const auto target = base::make_weak(thread);
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_nagram_menu_copy_confirm(
				lt_recipient,
				rpl::single(thread->chatListName())),
			.confirmed = [=](Fn<void()> &&close) {
				close();
				send(target);
			},
		}));
		return false;
	};
	*chooser = Window::ShowChooseRecipientBox(
		controller,
		std::move(chosen),
		tr::lng_nagram_menu_forward_without_quote());
}

void ShowForwardWithoutQuote(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	const auto item = controller->session().data().message(itemId);
	if (CopyOnly(item)) {
		ShowForwardCopy(controller, itemId);
		return;
	} else if (!CanForward(item)) {
		return;
	}
	Window::ShowForwardMessagesBox(controller, Data::ForwardDraft{
		.ids = item->history()->owner().itemOrItsGroup(item),
		.options = Data::ForwardOptions::NoSenderNames,
	});
}

int InsertPosition(not_null<Ui::PopupMenu*> menu) {
	for (auto index = int(menu->actions().size()); index != 0;) {
		--index;
		const auto value = menu->actions()[index]->property(kActionIdProperty);
		if (value.isValid()
			&& value.toInt() == static_cast<int>(ActionId::Forward)) {
			return index + 1;
		}
	}
	for (auto index = 0; index != int(menu->actions().size()); ++index) {
		const auto value = menu->actions()[index]->property(kActionIdProperty);
		const auto id = value.isValid() ? ActionId(value.toInt()) : ActionId();
		if (id == ActionId::Delete
			|| id == ActionId::Report
			|| id == ActionId::Select
			|| id == ActionId::BlockSender) {
			return index;
		}
	}
	return EndPosition(menu);
}

void Insert(
		not_null<Ui::PopupMenu*> menu,
		int position,
		not_null<Window::SessionController*> controller,
		FullMsgId itemId,
		ActionId id,
		const QString &text) {
	const auto target = Destination(menu, id);
	if (target != menu) {
		menu = target;
		position = EndPosition(menu);
	} else {
		position = std::min(position, EndPosition(menu));
	}
	const auto forward = (id == ActionId::Forward)
		|| (id == ActionId::ForwardWithoutQuote);
	const auto callback = crl::guard(controller, [=] {
		if (forward) {
			ShowForwardWithoutQuote(controller, itemId);
		} else if (ForDevice().Get(kConfirmRepeat)) {
			const auto weak = base::make_weak(controller);
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_menu_repeat_confirm_text(),
				.confirmed = [=](Fn<void()> &&close) {
					close();
					if (const auto strong = weak.get()) {
						SendRepeat(strong, itemId, id);
					}
				},
			}));
		} else {
			SendRepeat(controller, itemId, id);
		}
	});
	const auto action = Ui::Menu::CreateAction(menu, text, callback);
	const auto icon = forward ? &st::menuIconForward : &st::menuIconRepeat;
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action, icon, icon);
	Tag(menu->insertAction(position, std::move(widget)), id);
}

} // namespace

void InsertRepeatActions(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller) {
	const auto copyOnly = CopyOnly(item);
	if ((!copyOnly && !CanForward(item)) || !menu || !controller) {
		return;
	}
	const auto itemId = item->fullId();
	const auto forward = tr::lng_nagram_menu_forward_without_quote(tr::now);
	const auto copy = tr::lng_nagram_menu_repeat_as_copy(tr::now);
	const auto repeat = Available(controller, item)
		&& (!item->history()->peer->isBroadcast()
			|| !ForDevice().Get(kNoRepeatInChannels));
	auto position = InsertPosition(menu);
	if (copyOnly) {
		const auto forwardId = Shown(ActionId::Forward)
			? ActionId::Forward
			: ActionId::ForwardWithoutQuote;
		Insert(menu, position++, controller, itemId, forwardId, forward);
		if (repeat) {
			const auto repeatId = Shown(ActionId::Repeat)
				? ActionId::Repeat
				: ActionId::RepeatAsCopy;
			Insert(menu, position, controller, itemId, repeatId, copy);
		}
		return;
	}
	if (repeat) {
		Insert(menu, position++, controller, itemId, ActionId::Repeat,
			tr::lng_nagram_menu_repeat(tr::now));
		Insert(menu, position++, controller, itemId, ActionId::RepeatAsCopy,
			copy);
	}
	Insert(menu, position, controller, itemId, ActionId::ForwardWithoutQuote,
		forward);
}

} // namespace Nagram::Menu
