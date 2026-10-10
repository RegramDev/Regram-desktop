#include "regram/nagram/chats/cleanup.h"

#include "regram/nagram/chats/cleanup_model.h"
#include "regram/nagram/chats/local_pins.h"
#include "apiwrap.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QLocale>

#include <array>
#include <deque>

namespace Nagram::Chats {
namespace {

using Cleanup::Category;

constexpr auto kActionInterval = crl::time(1000);
constexpr auto kMaxShown = 200;

struct Candidate {
	not_null<History*> history;
	Category category = Category::InactiveChat;
	TimeId lastActivity = 0;
	Ui::Checkbox *check = nullptr;
};

[[nodiscard]] Cleanup::Facts FactsOf(not_null<History*> history) {
	const auto peer = history->peer;
	const auto user = peer->asUser();
	const auto chat = peer->asChat();
	const auto channel = peer->asChannel();
	const auto managed = (chat
		&& (chat->amCreator() || chat->hasAdminRights()))
		|| (channel && (channel->amCreator() || channel->hasAdminRights()));
	const auto special = peer->isSelf()
		|| peer->isServiceUser()
		|| peer->isRepliesChat()
		|| peer->isVerifyCodes()
		|| (user && user->isSupport());
	return {
		.user = (user != nullptr),
		.deleted = user && user->isInaccessible(),
		.bot = user && user->isBot(),
		.kept = managed
			|| special
			|| !history->inChatList()
			|| (history->folder() != nullptr)
			|| ShowsPinnedIcon(history, FilterId()),
		.lastActivity = history->chatListTimeId(),
	};
}

[[nodiscard]] std::optional<Category> CategoryOf(not_null<History*> history) {
	return Cleanup::Classify(FactsOf(history), base::unixtime::now());
}

[[nodiscard]] std::vector<Candidate> Collect(
		not_null<Main::Session*> session) {
	auto result = std::vector<Candidate>();
	for (const auto &row : session->data().chatsList()->indexed()->all()) {
		if (const auto history = row->history()) {
			if (const auto category = CategoryOf(history)) {
				result.push_back({
					.history = history,
					.category = *category,
					.lastActivity = history->chatListTimeId(),
				});
			}
		}
	}
	std::ranges::stable_sort(result, {}, &Candidate::lastActivity);
	return result;
}

[[nodiscard]] rpl::producer<QString> Title(Category category) {
	switch (category) {
	case Category::DeletedAccount: return tr::lng_nagram_cleanup_deleted();
	case Category::UnusedBot: return tr::lng_nagram_cleanup_bots();
	case Category::InactiveChat: return tr::lng_nagram_cleanup_inactive();
	}
	Unexpected("Category in Chats::Title.");
}

[[nodiscard]] QString Label(const Candidate &candidate) {
	const auto name = candidate.history->peer->name();
	if (candidate.lastActivity <= 0) {
		return name;
	}
	const auto date = base::unixtime::parse(candidate.lastActivity).date();
	return name
		+ u" · "_q
		+ QLocale().toString(date, QLocale::ShortFormat);
}

void Apply(not_null<History*> history, bool remove) {
	const auto peer = history->peer;
	auto &api = peer->session().api();
	if (!remove) {
		api.toggleHistoryArchived(history, true, [] {});
	} else if (const auto channel = peer->asChannel()) {
		api.leaveChannel(channel);
	} else {
		api.deleteConversation(peer, false);
	}
}

void CleanupBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	struct State {
		std::vector<Candidate> candidates;
		std::deque<not_null<History*>> queue;
		base::Timer timer;
		int total = 0;
		int skipped = 0;
		bool remove = false;
	};
	const auto titleMargin = style::margins(
		st::boxRowPadding.left(),
		st::boxMediumSkip,
		st::boxRowPadding.right(),
		0);
	const auto rowMargin = style::margins(
		st::boxRowPadding.left(),
		st::boxLittleSkip,
		st::boxRowPadding.right(),
		0);
	box->setTitle(tr::lng_nagram_cleanup());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_cleanup_about(),
		st::boxDividerLabel));
	const auto state = box->lifetime().make_state<State>();
	state->candidates = Collect(&controller->session());
	if (state->candidates.empty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_cleanup_empty(),
			st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	if (int(state->candidates.size()) > kMaxShown) {
		state->candidates.erase(
			state->candidates.begin() + kMaxShown,
			state->candidates.end());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_cleanup_limited(
				lt_amount,
				rpl::single(QString::number(kMaxShown))),
			st::boxDividerLabel));
	}
	for (const auto category : std::array{
		Category::DeletedAccount,
		Category::UnusedBot,
		Category::InactiveChat,
	}) {
		auto header = false;
		for (auto &candidate : state->candidates) {
			if (candidate.category != category) {
				continue;
			} else if (!header) {
				header = true;
				box->addRow(
					object_ptr<Ui::FlatLabel>(
						box,
						Title(category),
						st::boxLabel),
					titleMargin);
			}
			candidate.check = box->addRow(
				object_ptr<Ui::Checkbox>(
					box,
					Label(candidate),
					category == Category::DeletedAccount),
				rowMargin);
		}
	}
	const auto progress = box->addRow(
		object_ptr<Ui::FlatLabel>(box, QString(), st::boxDividerLabel),
		titleMargin);
	const auto step = [=] {
		if (state->queue.empty()) {
			state->timer.cancel();
			return;
		}
		const auto history = state->queue.front();
		state->queue.pop_front();
		// The chat may have changed since the list was built.
		if (CategoryOf(history)) {
			Apply(history, state->remove);
		} else {
			++state->skipped;
		}
		const auto left = int(state->queue.size());
		progress->setText(left
			? tr::lng_nagram_cleanup_progress(
				tr::now,
				lt_done,
				QString::number(state->total - left),
				lt_total,
				QString::number(state->total))
			: tr::lng_nagram_cleanup_finished(
				tr::now,
				lt_done,
				QString::number(state->total - state->skipped),
				lt_skipped,
				QString::number(state->skipped)));
		if (!left) {
			state->timer.cancel();
		}
	};
	state->timer.setCallback(step);
	const auto selected = [=] {
		auto result = 0;
		for (const auto &candidate : state->candidates) {
			const auto check = candidate.check;
			result += (check && check->checked() && !check->isDisabled());
		}
		return result;
	};
	const auto run = [=](bool remove) {
		if (!state->queue.empty()) {
			box->showToast(tr::lng_nagram_cleanup_busy(tr::now));
			return;
		}
		for (const auto &candidate : state->candidates) {
			const auto check = candidate.check;
			if (check && check->checked() && !check->isDisabled()) {
				check->setDisabled(true);
				state->queue.push_back(candidate.history);
			}
		}
		state->total = int(state->queue.size());
		state->skipped = 0;
		state->remove = remove;
		state->timer.callEach(kActionInterval);
		step();
	};
	const auto weak = base::make_weak(box);
	const auto start = [=](bool remove) {
		if (controller->showFrozenError()) {
			return;
		} else if (!state->queue.empty()) {
			box->showToast(tr::lng_nagram_cleanup_busy(tr::now));
			return;
		}
		const auto count = selected();
		if (!count) {
			box->showToast(tr::lng_nagram_cleanup_none_selected(tr::now));
		} else if (!remove) {
			run(false);
		} else {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_cleanup_delete_sure(
					lt_amount,
					rpl::single(QString::number(count))),
				.confirmed = [=](Fn<void()> close) {
					close();
					if (weak) {
						run(true);
					}
				},
				.confirmText = tr::lng_nagram_cleanup_delete(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		}
	};
	box->addButton(tr::lng_nagram_cleanup_archive(), [=] { start(false); });
	box->addButton(tr::lng_nagram_cleanup_delete(), [=] { start(true); });
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

void ShowCleanup(not_null<Window::SessionController*> controller) {
	controller->show(Box(CleanupBox, controller));
}

} // namespace Nagram::Chats
