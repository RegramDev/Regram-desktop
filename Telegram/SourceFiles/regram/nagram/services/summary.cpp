#include "regram/nagram/services/summary.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/nagram/menu/actions.h"
#include "regram/nagram/privacy/protection.h"
#include "regram/nagram/services/request.h"
#include "regram/nagram/services/summary_model.h"
#include "spellcheck/spellcheck_types.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Nagram {
namespace {

struct Collected {
	SummaryScope scope;
	bool copyAllowed = true;
};

[[nodiscard]] std::optional<ServiceDefinition> SummaryService() {
	const auto config = Services();
	return config
		? FindService(*config, config->value(u"summary"_q).toString())
		: std::nullopt;
}

[[nodiscard]] Collected Collect(
		not_null<Main::Session*> session,
		const MessageIdsList &ids) {
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto &id : ids) {
		if (const auto item = session->data().message(id)) {
			items.push_back(item);
		}
	}
	ranges::sort(items, [](const auto &a, const auto &b) {
		return (a->date() != b->date())
			? (a->date() < b->date())
			: (a->fullId() < b->fullId());
	});
	auto result = Collected();
	auto texts = QStringList();
	for (const auto &item : items) {
		const auto &text = item->originalText().text;
		if (!text.trimmed().isEmpty()) {
			texts.push_back(text);
			result.copyAllowed = result.copyAllowed
				&& (item->allowsForward() || Privacy::ForceCopy());
		}
	}
	result.scope = PlanSummary(texts);
	return result;
}

void SummaryBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		MessageIdsList ids,
		ServiceDefinition service,
		QByteArray config) {
	struct State {
		ServiceRequest request;
		QString result;
		bool loading = false;
	};
	box->setTitle(tr::lng_nagram_summary_title());
	const auto state = box->lifetime().make_state<State>();
	const auto initial = Collect(session, ids);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_summary_about(
			tr::now,
			lt_amount,
			QString::number(initial.scope.texts.size()),
			lt_chars,
			QString::number(initial.scope.characters),
			lt_name,
			service.name,
			lt_url,
			ServiceEndpoint(service).toDisplayString())
			+ (initial.scope.truncated
				? (u"\n\n"_q + tr::lng_nagram_summary_truncated(tr::now))
				: QString()),
		st::boxLabel));
	const auto label = box->addRow(
		object_ptr<Ui::FlatLabel>(box, st::boxLabel));
	label->setSelectable(initial.copyAllowed);
	box->addButton(tr::lng_nagram_summary_generate(), [=] {
		if (state->loading) {
			return;
		} else if (ForDevice().Get(kServicesConfig) != config) {
			label->setText(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		const auto current = Collect(session, ids);
		if (current.scope.texts.isEmpty()) {
			label->setText(tr::lng_nagram_summary_missing(tr::now));
			return;
		}
		state->loading = true;
		state->result = QString();
		label->setText(tr::lng_nagram_service_testing(tr::now));
		state->request.json(
			service,
			BuildSummaryBody(
				service,
				current.scope.texts,
				Core::App().settings().translateTo().twoLetterCode()),
			crl::guard(box, [=](ServiceResult response) {
				state->loading = false;
				if (response.error != ServiceError::None) {
					label->setText(
						ServiceErrorText(response.error, response.status));
					return;
				}
				const auto text = ParseSummaryResult(service, response.body);
				if (!text) {
					label->setText(ServiceErrorText(ServiceError::Response));
					return;
				}
				state->result = *text;
				label->setText(*text);
			}));
	});
	if (initial.copyAllowed) {
		box->addButton(tr::lng_context_copy_text(), [=] {
			if (!state->result.isEmpty()
				&& Collect(session, ids).copyAllowed) {
				TextUtilities::SetClipboardText(
					TextForMimeData::Simple(state->result));
			}
		});
	}
	box->addButton(tr::lng_cancel(), [=] {
		state->request.cancel();
		box->closeBox();
	});
}

} // namespace

void InsertSummaryAction(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller,
		MessageIdsList selected) {
	if (!menu || !controller || (!item && selected.empty())) {
		return;
	}
	const auto service = SummaryService();
	if (!service) {
		return;
	}
	const auto session = &controller->session();
	const auto ids = selected.empty()
		? MessageIdsList{ item->fullId() }
		: selected;
	if (Collect(session, ids).scope.texts.isEmpty()) {
		return;
	}
	const auto action = Ui::Menu::CreateAction(
		menu,
		tr::lng_nagram_menu_summarize(tr::now),
		crl::guard(controller, [=] {
			const auto current = SummaryService();
			if (!current) {
				controller->showToast(tr::lng_nagram_service_invalid(tr::now));
				return;
			}
			controller->show(Box(
				SummaryBox,
				session,
				ids,
				*current,
				ForDevice().Get(kServicesConfig)));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(),
		menu->menu()->st(),
		action,
		&st::menuIconChatBubble,
		&st::menuIconChatBubble);
	Menu::Tag(
		menu->insertAction(Menu::EndPosition(menu), std::move(widget)),
		Menu::ActionId::Summarize);
}

} // namespace Nagram
