#include "regram/nagram/menu/draft.h"

#include "data/data_changes.h"
#include "data/data_drafts.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "window/window_session_controller.h"

namespace Nagram::Menu {

bool DraftOccupied(not_null<Data::Thread*> target) {
	const auto history = target->owningHistory();
	const auto topic = target->topicRootId();
	const auto monoforum = target->monoforumPeerId();
	auto &local = target->session().local();
	local.readDraftsWithCursors(history);
	return !Data::DraftIsNull(history->localDraft(topic, monoforum))
		|| !Data::DraftIsNull(history->cloudDraft(topic, monoforum))
		|| history->localEditDraft(topic, monoforum)
		|| !history->forwardDraft(topic, monoforum).ids.empty();
}

void PlaceTextDraft(
		not_null<Window::SessionController*> controller,
		not_null<Data::Thread*> target,
		TextWithTags text,
		FullMsgId replyTo) {
	const auto history = target->owningHistory();
	const auto topic = target->topicRootId();
	const auto monoforum = target->monoforumPeerId();
	const auto cursor = int(text.text.size());
	history->setLocalDraft(std::make_unique<Data::Draft>(text, FullReplyTo{
		.messageId = replyTo,
		.topicRootId = topic,
		.monoforumPeerId = monoforum,
	}, SuggestOptions(), MessageCursor{
		cursor, cursor, Ui::kQFixedMax,
	}, Data::WebPageDraft()));
	controller->session().changes().entryUpdated(
		target, Data::EntryUpdate::Flag::LocalDraftSet);
	auto params = Window::SectionShow();
	params.reapplyLocalDraft = true;
	controller->showThread(target, ShowAtTheEndMsgId, params);
}

} // namespace Nagram::Menu
