#include "regram/nagram/menu/batch.h"

#include "regram/nagram/menu/actions.h"
#include "regram/nagram/menu/draft.h"
#include "data/data_chat_participant_status.h"
#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram::Menu {
namespace {

constexpr auto kMaximumMessages = 100;
constexpr auto kMaximumText = 32768;

bool Available(HistoryItem *item) {
	return item && item->allowsForward() && !item->isTtlCoveredMedia()
		&& !item->isEphemeral() && !item->isMediaSensitive();
}

std::optional<TextWithEntities> Prepare(
		not_null<Main::Session*> session,
		MessageIdsList ids,
		bool reverse) {
	if (ids.size() < 2 || ids.size() > kMaximumMessages) {
		return std::nullopt;
	}
	if (base::flat_set<FullMsgId>(ids.begin(), ids.end()).size() != ids.size()) {
		return std::nullopt;
	}
	if (reverse) {
		ranges::reverse(ids);
	}
	auto text = TextWithEntities();
	for (const auto &id : ids) {
		const auto item = session->data().message(id);
		if (!Available(item)) {
			return std::nullopt;
		}
		if (!text.empty()) {
			text.append(u"\n\n"_q);
		}
		text.append(item->clipboardText().rich);
		if (text.text.size() > kMaximumText) {
			return std::nullopt;
		}
	}
	return text;
}

void BatchBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids,
		FullMsgId sourceId) {
	box->setTitle(tr::lng_nagram_menu_batch());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_menu_batch_preview(), st::boxLabel));
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::MultiLine));
	field->setMaxLength(kMaximumText);
	const auto reverse = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_menu_batch_reverse(tr::now), false));
	const auto omitNames = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_menu_batch_no_names(tr::now), false));
	const auto prepared = box->lifetime().make_state<std::optional<TextWithEntities>>();
	const auto rebuild = [=] {
		*prepared = Prepare(&controller->session(), ids, reverse->checked());
		field->setTextWithTags(*prepared
			? TextWithTags{ (*prepared)->text,
				TextUtilities::ConvertEntitiesToTextTags((*prepared)->entities) }
			: TextWithTags());
	};
	reverse->checkedChanges() | rpl::on_next(rebuild, box->lifetime());
	const auto validate = [=] {
		const auto current = Prepare(&controller->session(), ids, reverse->checked());
		if (!current || !*prepared || *current != **prepared) {
			box->showToast(tr::lng_nagram_menu_batch_changed(tr::now));
			return false;
		}
		return true;
	};
	const auto draft = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_nagram_menu_batch_draft(), st::settingsButtonNoIcon));
	draft->setClickedCallback(crl::guard(controller, [=] {
		if (!validate()) {
			return;
		}
		const auto source = controller->session().data().message(sourceId);
		if (!source) {
			box->showToast(tr::lng_nagram_menu_batch_changed(tr::now));
			return;
		}
		const auto target = source->topic()
			? static_cast<Data::Thread*>(source->topic())
			: static_cast<Data::Thread*>(source->history());
		if (DraftOccupied(target)) {
			box->showToast(tr::lng_nagram_menu_batch_draft_exists(tr::now));
			return;
		}
		if (!Data::CanSendTexts(target)) {
			box->showToast(tr::lng_nagram_menu_batch_unavailable(tr::now));
			return;
		}
		const auto entered = field->getTextWithTags();
		if (entered.text.isEmpty()) {
			box->showToast(tr::lng_nagram_menu_batch_no_text(tr::now));
			return;
		}
		const auto weakBox = base::make_weak(box);
		PlaceTextDraft(controller, target, entered);
		if (weakBox) {
			weakBox->closeBox();
		}
	}));
	const auto saved = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_nagram_menu_batch_saved(), st::settingsButtonNoIcon));
	saved->setClickedCallback(crl::guard(controller, [=] {
		if (!validate()) {
			return;
		}
		const auto target = controller->session().data().history(
			controller->session().user());
		if (DraftOccupied(target)) {
			box->showToast(tr::lng_nagram_menu_batch_draft_exists(tr::now));
			return;
		}
		auto ordered = ids;
		if (reverse->checked()) {
			ranges::reverse(ordered);
		}
		if (controller->content()->setForwardDraft(target, {
			.ids = std::move(ordered),
			.options = omitNames->checked()
				? Data::ForwardOptions::NoSenderNames
				: Data::ForwardOptions::PreserveInfo,
		})) {
			box->closeBox();
		} else {
			box->showToast(tr::lng_nagram_menu_batch_unavailable(tr::now));
		}
	}));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	rebuild();
}

void Insert(
		not_null<Ui::PopupMenu*> menu,
		int position,
		ActionId id,
		const QString &text,
		Fn<void()> callback) {
	const auto target = Destination(menu, id);
	if (target != menu) {
		menu = target;
		position = EndPosition(menu);
	} else {
		position = std::min(position, EndPosition(menu));
	}
	const auto action = Ui::Menu::CreateAction(menu, text, std::move(callback));
	const auto icon = (id == ActionId::Batch)
		? &st::menuIconCopy : &st::menuIconSelect;
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action, icon, icon);
	Tag(menu->insertAction(position, std::move(widget)), id);
}

} // namespace

void InsertBatchActions(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller,
		MessageIdsList selected,
		Fn<void(HistoryItem*)> select) {
	if (!menu || !controller) {
		return;
	}
	auto position = EndPosition(menu);
	if (selected.size() > 1 && selected.size() <= kMaximumMessages) {
		const auto sourceId = selected.front();
		Insert(menu, position++, ActionId::Batch,
			tr::lng_nagram_menu_batch(tr::now),
			crl::guard(controller, [=] {
				controller->show(Box(BatchBox, controller, selected, sourceId));
			}));
	}
	if (selected.size() > 1 && select) {
		Insert(menu, position++, ActionId::SelectBetween,
			tr::lng_nagram_menu_select_between(tr::now),
			crl::guard(controller, [=] { select(nullptr); }));
	}
	if (item && select && item->canBeSelected()) {
		const auto itemId = item->fullId();
		Insert(menu, position, ActionId::SelectSender,
			tr::lng_nagram_menu_select_sender(tr::now),
			crl::guard(controller, [=] {
				if (const auto current = controller->session().data().message(itemId)) {
					select(current);
				}
			}));
	}
}

} // namespace Nagram::Menu
