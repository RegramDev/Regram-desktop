#ifdef _DEBUG

#include "test/nagram_menu_scenario.h"

#include "regram/nagram/compose/options.h"
#include "regram/nagram/compose/text.h"
#include "regram/nagram/core/options.h"
#include "regram/nagram/menu/model.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/messages/reading.h"
#include "test/test_log.h"
#include "test/test_menu.h"
#include "test/test_widgets.h"

#include "core/application.h"
#include "data/data_history_messages.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_inner_widget.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "history/view/history_view_list_widget.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "lang/lang_keys.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "window/main_window.h"

#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <memory>
#include <optional>

namespace Nagram::Tests {
namespace {

struct MenuState {
	MsgId messageId = 0;
	QPointer<QWidget> target;
	QPointer<Ui::InputField> field;
	QPointer<Ui::PopupMenu> menu;
	QByteArray originalMenu;
	QByteArray originalReplies;
	int originalChinese = 0;
	bool configured = false;
	bool changedDraft = false;
	bool expectReading = false;
};

Window::SessionController *Controller() {
	const auto window = Core::App().activeWindow();
	return window ? window->sessionController() : nullptr;
}

History *SavedHistory() {
	const auto controller = Controller();
	return controller
		? controller->session().data().history(
			controller->session().userPeerId()).get()
		: nullptr;
}

bool HasHan(const QString &text) {
	for (const auto &character : text) {
		if (character.script() == QChar::Script_Han) {
			return true;
		}
	}
	return false;
}

MsgId ChooseMessage(not_null<History*> history) {
	const auto requested = qEnvironmentVariable("NAGRAM_TEST_MENU_MESSAGE_ID");
	if (!requested.isEmpty()) {
		auto valid = false;
		const auto id = requested.toInt(&valid);
		return valid && id > 0 ? MsgId(id) : MsgId(0);
	}
	const auto last = history->lastMessage();
	if (!last) {
		return 0;
	}
	const auto snapshot = history->messages().snapshot({ last->id, 100, 0 });
	auto fallback = MsgId(0);
	for (auto it = snapshot.messageIds.rbegin();
			it != snapshot.messageIds.rend(); ++it) {
		const auto item = history->owner().message(history->peer, *it);
		if (!item || item->isService() || item->originalText().empty()) {
			continue;
		}
		if (!fallback) {
			fallback = *it;
		}
		const auto &text = item->translatedTextWithLocalEntities();
		const auto projected = Messages::ProjectReading(text, false, 2);
		if (HasHan(text.text) && projected && projected->text != text.text) {
			return *it;
		}
	}
	return fallback;
}

std::optional<QPoint> MessagePointList(
	not_null<HistoryView::ListWidget*> list,
	MsgId messageId);
std::optional<QPoint> MessagePointInner(
	not_null<HistoryInner*> inner,
	MsgId messageId);

HistoryView::ListWidget *FindMessageList(MsgId messageId) {
	const auto controller = Controller();
	if (!controller || !messageId) {
		return nullptr;
	}
	const auto item = controller->session().data().message(
		controller->session().userPeerId(), messageId);
	if (!item) {
		return nullptr;
	}
	for (const auto &list : Test::FindVisible<HistoryView::ListWidget>(
			controller->widget())) {
		const auto view = list->viewByPosition(item->position());
		if (view && view->data() == item && view->delegate() == list) {
			return list;
		}
	}
	return nullptr;
}

HistoryInner *FindMessageInner(MsgId messageId) {
	const auto controller = Controller();
	if (!controller || !messageId) {
		return nullptr;
	}
	const auto item = controller->session().data().message(
		controller->session().userPeerId(), messageId);
	if (!item) {
		return nullptr;
	}
	for (const auto &inner : Test::FindVisible<HistoryInner>(
			controller->widget())) {
		if (const auto view = inner->viewByItem(item)) {
			if (view->data() == item) {
				return inner;
			}
		}
	}
	return nullptr;
}

QWidget *FindMessageTarget(MsgId messageId) {
	if (const auto list = FindMessageList(messageId)) {
		return list;
	}
	return FindMessageInner(messageId);
}

std::optional<QPoint> MessagePoint(
		not_null<QWidget*> target,
		MsgId messageId) {
	if (const auto list = dynamic_cast<HistoryView::ListWidget*>(target.get())) {
		return MessagePointList(list, messageId);
	} else if (const auto inner = dynamic_cast<HistoryInner*>(target.get())) {
		return MessagePointInner(inner, messageId);
	}
	return std::nullopt;
}

QString MessageViewDetails(MsgId messageId) {
	const auto controller = Controller();
	if (!controller) {
		return u"controller missing"_q;
	}
	const auto item = controller->session().data().message(
		controller->session().userPeerId(), messageId);
	const auto lists = Test::FindVisible<HistoryView::ListWidget>(
		controller->widget());
	const auto legacy = Test::FindVisible<HistoryInner>(controller->widget());
	auto result = u"item=%1 lists=%2 legacy=%3"_q
		.arg(item ? 1 : 0).arg(lists.size()).arg(legacy.size());
	for (const auto &list : lists) {
		const auto view = item ? list->viewByPosition(item->position()) : nullptr;
		result += u" listVisible=%1 view=%2 height=%3 point=%4"_q
			.arg(list->isVisible()).arg(view ? 1 : 0)
			.arg(view ? view->height() : 0)
			.arg(MessagePointList(list, messageId).has_value() ? 1 : 0);
	}
	for (const auto &inner : legacy) {
		const auto view = item ? inner->viewByItem(item) : nullptr;
		result += u" innerView=%1 top=%2 point=%3"_q
			.arg(view ? 1 : 0).arg(view ? inner->itemTop(view) : -1)
			.arg(MessagePointInner(inner, messageId).has_value() ? 1 : 0);
	}
	return result;
}

std::optional<QPoint> MessagePointList(
		not_null<HistoryView::ListWidget*> list,
		MsgId messageId) {
	const auto item = list->session().data().message(
		list->session().userPeerId(), messageId);
	const auto view = item ? list->viewByPosition(item->position()) : nullptr;
	if (!view || view->data() != item || view->height() <= 0) {
		return std::nullopt;
	}
	auto first = -1;
	for (auto y = 0; y != list->height(); ++y) {
		if (list->elementIntersectsRange(view, y, y + 1)) {
			first = y;
			break;
		}
	}
	if (first < 0) {
		return std::nullopt;
	}
	const auto inner = view->innerGeometry();
	const auto x = std::clamp(inner.center().x(), 0, list->width() - 1);
	const auto y = std::clamp(
		first + inner.center().y(), first,
		std::min(first + view->height() - 1, list->height() - 1));
	return QPoint(x, y);
}

std::optional<QPoint> MessagePointInner(
		not_null<HistoryInner*> inner,
		MsgId messageId) {
	const auto item = inner->session().data().message(
		inner->session().userPeerId(), messageId);
	const auto view = item ? inner->viewByItem(item) : nullptr;
	const auto top = view ? inner->itemTop(view) : -1;
	if (!view || top < 0 || view->height() <= 0
		|| top >= inner->height() || top + view->height() <= 0) {
		return std::nullopt;
	}
	const auto rect = view->innerGeometry();
	return QPoint(
		std::clamp(rect.center().x(), 0, inner->width() - 1),
		std::clamp(top + rect.center().y(), 0, inner->height() - 1));
}

Ui::PopupMenu *VisibleMenu() {
	for (const auto &widget : QApplication::topLevelWidgets()) {
		if (const auto menu = dynamic_cast<Ui::PopupMenu*>(widget)) {
			if (Test::PopupMenuReady(menu)) {
				return menu;
			}
		}
	}
	return nullptr;
}

void LogMenu(not_null<Ui::PopupMenu*> menu, const QString &kind) {
	for (const auto &action : menu->actions()) {
		const auto marker = action->property("nagramMenuActionId");
		Test::Note(u"NAGRAM_MENU kind=%1 id=%2 text=%3 separator=%4"_q.arg(
			kind,
			marker.isValid() ? QString::number(marker.toInt()) : u"none"_q,
			action->text(),
			action->isSeparator() ? u"1"_q : u"0"_q));
	}
}

bool HasAction(not_null<Ui::PopupMenu*> menu, Menu::ActionId id) {
	return ranges::any_of(menu->actions(), [=](QAction *action) {
		return action->property("nagramMenuActionId").toInt()
			== static_cast<int>(id);
	});
}

bool HasText(not_null<Ui::PopupMenu*> menu, const QString &text) {
	return ranges::any_of(menu->actions(), [&](QAction *action) {
		return action->text().contains(text);
	});
}

void SendContextMenu(not_null<QWidget*> widget, QPoint local) {
	auto event = QContextMenuEvent(
		QContextMenuEvent::Mouse, local, widget->mapToGlobal(local));
	QApplication::sendEvent(widget, &event);
}

} // namespace

void AppendMenuScenario(not_null<Test::Runner*> runner) {
	if (qEnvironmentVariable("NAGRAM_TEST_SCENARIO") != u"menu"_q) {
		return;
	}
	const auto state = std::make_shared<MenuState>();
	runner->waitEvent(u"launch_finished"_q);
	runner->waitForSessionReady();
	runner->add({
		.name = u"open saved messages for menu inspection"_q,
		.run = [] {
			if (const auto controller = Controller()) {
				controller->showPeerHistory(
					controller->session().userPeerId());
			}
		},
		.until = [] {
			const auto history = SavedHistory();
			return history && history->lastMessage();
		},
		.then = [=] {
			const auto history = SavedHistory();
			state->messageId = history ? ChooseMessage(history) : MsgId(0);
			const auto item = history && state->messageId
				? history->owner().message(history->peer, state->messageId)
				: nullptr;
			if (item) {
				const auto &text = item->translatedTextWithLocalEntities();
				const auto projected = Messages::ProjectReading(text, false, 2);
				state->expectReading = projected && projected->text != text.text;
			}
			Test::Check(state->messageId > 0,
				u"saved messages contains a text message for menu inspection"_q,
				u"messageId=%1"_q.arg(state->messageId.bare));
			if (state->messageId) {
				Test::Note(u"NAGRAM_MENU reading_sample=%1"_q.arg(
					state->expectReading));
				Controller()->showPeerHistory(
					Controller()->session().userPeerId(),
					Window::SectionShow::Way::ClearStack,
					state->messageId);
			}
		},
	});
	runner->add({
		.name = u"resolve selected message view"_q,
		.skipReason = [=] {
			return state->messageId ? QString()
				: u"no text message was loaded"_q;
		},
		.until = [=] {
			const auto target = FindMessageTarget(state->messageId);
			return target && MessagePoint(target, state->messageId).has_value();
		},
		.then = [=] {
			state->target = FindMessageTarget(state->messageId);
			Test::Note(u"NAGRAM_MENU selected_message=%1"_q.arg(
				state->messageId.bare));
		},
		.timeoutDetails = [=] {
			return MessageViewDetails(state->messageId);
		},
	});
	runner->add({
		.name = u"configure disposable menu fixtures"_q,
		.run = [=] {
			state->originalMenu = ForDevice().Get(Menu::kMenuConfig);
			state->originalReplies = ForDevice().Get(Compose::kQuickReplies);
			state->originalChinese = ForDevice().Get(Messages::kReadingChinese);
			auto menu = state->originalMenu;
			for (const auto &id : { Menu::ActionId::Repeat,
					Menu::ActionId::RepeatAsCopy,
					Menu::ActionId::ForwardWithoutQuote,
					Menu::ActionId::Screenshot,
					Menu::ActionId::FilterAuthor }) {
				menu = Menu::WriteVisibility(menu, id, Menu::Visibility::Show);
			}
			state->configured = true;
			Test::Check(ForDevice().Set(Menu::kMenuConfig, menu)
				&& Compose::SetQuickReplies({ u"Probe one"_q, u"Probe two"_q })
				&& ForDevice().Set(Messages::kReadingChinese, 2),
				u"disposable menu fixtures configured"_q);
		},
	});
	runner->onFinish([=] {
		if (!state->configured) {
			return;
		}
		const auto menuRestored = ForDevice().Set(
			Menu::kMenuConfig, state->originalMenu);
		const auto repliesRestored = ForDevice().Set(
			Compose::kQuickReplies, state->originalReplies);
		const auto chineseRestored = ForDevice().Set(
			Messages::kReadingChinese, state->originalChinese);
		Test::Check(menuRestored && repliesRestored && chineseRestored,
			u"disposable menu fixtures restored"_q);
		if (state->changedDraft && state->field) {
			state->field->setText(QString());
		}
	});
	runner->add({
		.name = u"open selected message context menu"_q,
		.run = [=] {
			if (state->target) {
				if (const auto point = MessagePoint(
						state->target, state->messageId)) {
					SendContextMenu(state->target, *point);
				}
			}
		},
		.until = [] { return VisibleMenu() != nullptr; },
		.then = [=] {
			state->menu = VisibleMenu();
			if (state->menu) {
				LogMenu(state->menu, u"message"_q);
				for (const auto &id : { Menu::ActionId::Repeat,
						Menu::ActionId::RepeatAsCopy,
						Menu::ActionId::ForwardWithoutQuote }) {
					Test::Check(HasAction(state->menu, id),
						u"configured Nagram message action is present"_q,
						u"id=%1"_q.arg(static_cast<int>(id)));
				}
				for (auto value = int(Menu::ActionId::Batch);
						value <= int(Menu::ActionId::FilterAuthor); ++value) {
					Test::Note(u"NAGRAM_MENU action=%1 present=%2"_q
						.arg(value)
						.arg(HasAction(state->menu, Menu::ActionId(value))));
				}
				Test::Note(u"NAGRAM_MENU E24=configuration_not_action"_q);
				Test::Check(HasAction(state->menu, Menu::ActionId::Screenshot)
					&& HasAction(state->menu, Menu::ActionId::FilterAuthor),
					u"E21 screenshot and E23 author filter actions are present"_q);
				if (state->expectReading) {
					Test::Check(HasAction(state->menu, Menu::ActionId::Reading),
						u"E22 reading conversion action is present"_q);
				} else {
					Test::Note(u"NAGRAM_MENU E22=N/A no convertible text"_q);
				}
				state->menu->hideMenu(true);
			}
		},
	});
	runner->add({
		.name = u"open composer context menu"_q,
		.run = [=] {
			if (const auto controller = Controller()) {
				const auto fields = Test::FindVisible<Ui::InputField>(
					controller->widget());
				for (const auto &field : fields) {
					if (field->isEnabled() && (!state->field
						|| field->mapToGlobal(field->rect().bottomLeft()).y()
							> state->field->mapToGlobal(
								state->field->rect().bottomLeft()).y())) {
						state->field = field;
					}
				}
				if (state->field) {
					if (state->field->empty()) {
						state->field->setText(u"Nagram menu probe"_q);
						state->changedDraft = true;
					}
					SendContextMenu(state->field, state->field->rect().center());
				}
			}
		},
		.until = [] { return VisibleMenu() != nullptr; },
		.then = [=] {
			state->menu = VisibleMenu();
			if (state->menu) {
				LogMenu(state->menu, u"composer"_q);
				Test::Check(HasText(state->menu,
					tr::lng_nagram_quick_reply_insert(
						tr::now, lt_index, u"1"_q)),
					u"D21 quick reply action is present"_q);
				Test::Check(HasText(state->menu,
					tr::lng_nagram_translate_draft(tr::now)),
					u"draft translation action is present"_q);
				state->menu->hideMenu(true);
			}
		},
	});
}

} // namespace Nagram::Tests

#endif // _DEBUG
