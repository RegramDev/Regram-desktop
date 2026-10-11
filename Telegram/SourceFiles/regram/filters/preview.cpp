#include "regram/filters/preview.h"

#include "base/weak_ptr.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_history_messages.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "regram/filters/preview_policy.h"
#include "regram/filters/runtime.h"

#include <deque>
#include <map>

namespace Regram::Filters {
namespace {

constexpr auto kMaximumPreviewChats = 512;
constexpr auto kMaximumPreviewWork = 4;

struct PreviewEntry {
	base::weak_ptr<Data::Thread> thread;
	FullMsgId top;
	FullMsgId selected;
	std::vector<FullMsgId> candidates;
	uint64 serial = 0;
	mtpRequestId request = 0;
	bool ready = false;
	bool localTried = false;
	bool complete = false;
	bool working = false;
};

class PreviewCache final : public base::has_weak_ptr {
public:
	explicit PreviewCache(not_null<Main::Session*> session);
	[[nodiscard]] HistoryItem *get(
		not_null<Data::Thread*> thread,
		not_null<HistoryItem*> top);

private:
	void cancel(PreviewEntry &entry);
	void invalidate();
	void invalidateItem(not_null<const HistoryItem*> item);
	void schedule(quintptr key);
	void pump();
	void request(quintptr key, PreviewEntry &entry);
	void match(quintptr key, PreviewEntry &entry);
	void finish(quintptr key, uint64 serial, FullMsgId selected);

	const not_null<Main::Session*> _session;
	MTP::Sender _api;
	std::map<quintptr, PreviewEntry> _entries;
	std::deque<quintptr> _pending;
	uint64 _serial = 0;
	int _active = 0;
	bool _scheduled = false;
	rpl::lifetime _lifetime;

};

PreviewCache::PreviewCache(not_null<Main::Session*> session)
: _session(session)
, _api(&session->mtp()) {
	Updates(session) | rpl::on_next([=] { invalidate(); }, _lifetime);
	session->changes().messageUpdates(
		Data::MessageUpdate::Flag::Edited
			| Data::MessageUpdate::Flag::Destroyed
	) | rpl::on_next([=](const Data::MessageUpdate &update) {
		invalidateItem(update.item);
	}, _lifetime);
	session->data().itemRemoved() | rpl::on_next([=](auto item) {
		invalidateItem(item);
	}, _lifetime);
}

void PreviewCache::cancel(PreviewEntry &entry) {
	if (entry.request) {
		_api.request(base::take(entry.request)).cancel();
	}
	if (std::exchange(entry.working, false)) {
		--_active;
	}
	entry.serial = ++_serial;
	entry.ready = false;
}

void PreviewCache::invalidate() {
	for (auto &pair : _entries) {
		auto &entry = pair.second;
		cancel(entry);
		if (const auto thread = entry.thread.get()) {
			if (const auto selected = _session->data().message(entry.selected)) {
				thread->lastItemDialogsView().itemInvalidated(selected);
			}
			thread->updateChatListEntry();
		}
	}
}

void PreviewCache::invalidateItem(not_null<const HistoryItem*> item) {
	for (auto &pair : _entries) {
		auto &entry = pair.second;
		if (entry.top != item->fullId()
			&& entry.selected != item->fullId()
			&& !ranges::contains(entry.candidates, item->fullId())) {
			continue;
		}
		cancel(entry);
		if (const auto thread = entry.thread.get()) {
			thread->lastItemDialogsView().itemInvalidated(item);
			thread->updateChatListEntry();
		}
	}
}

HistoryItem *PreviewCache::get(
		not_null<Data::Thread*> thread,
		not_null<HistoryItem*> top) {
	const auto key = reinterpret_cast<quintptr>(thread.get());
	if (!_entries.contains(key) && _entries.size() >= kMaximumPreviewChats) {
		const auto oldest = ranges::min_element(
			_entries,
			{},
			[](const auto &pair) { return pair.second.serial; });
		cancel(oldest->second);
		_entries.erase(oldest);
	}
	auto &entry = _entries[key];
	if (!entry.thread || entry.top != top->fullId()) {
		cancel(entry);
		entry.thread = base::make_weak(thread);
		entry.top = top->fullId();
		entry.candidates.clear();
		entry.localTried = false;
		entry.complete = false;
	}
	if (!Hidden(top)) {
		entry.selected = top->fullId();
		entry.ready = true;
		return top;
	}
	const auto selected = _session->data().message(entry.selected);
	const auto usable = selected && !selected->isEmpty()
		&& selected->history() == top->history()
		&& selected->date() <= top->date()
		&& (!thread->topicRootId() || selected->topicRootId() == thread->topicRootId())
		&& !Hidden(selected);
	if (entry.ready && entry.selected && !usable) {
		cancel(entry);
	}
	if (!entry.ready && !entry.working) {
		schedule(key);
	}
	return usable ? selected : nullptr;
}

void PreviewCache::schedule(quintptr key) {
	if (!ranges::contains(_pending, key)) {
		_pending.push_back(key);
	}
	if (std::exchange(_scheduled, true)) {
		return;
	}
	crl::on_main(base::make_weak(this), [=] {
		_scheduled = false;
		pump();
	});
}

void PreviewCache::pump() {
	while (_active < kMaximumPreviewWork && !_pending.empty()) {
		const auto key = _pending.front();
		_pending.pop_front();
		const auto found = _entries.find(key);
		if (found == _entries.end() || !found->second.thread
			|| found->second.ready || found->second.working) {
			continue;
		}
		auto &entry = found->second;
		entry.working = true;
		++_active;
		if (entry.candidates.empty()) {
			request(key, entry);
		} else {
			match(key, entry);
		}
	}
}

void PreviewCache::request(quintptr key, PreviewEntry &entry) {
	const auto thread = entry.thread.get();
	const auto peer = thread->peer();
	if (!entry.localTried) {
		entry.localTried = true;
		const auto slice = thread->owningHistory()->messages().snapshot({
			entry.top.msg, kPreviewLookback, 0 });
		for (const auto id : slice.messageIds | ranges::views::reverse) {
			if (id < entry.top.msg) {
				entry.candidates.push_back({ entry.top.peer, id });
			}
		}
		if (!entry.candidates.empty()) {
			entry.complete = slice.skippedBefore == 0
				|| entry.candidates.size() >= kPreviewLookback;
			match(key, entry);
			return;
		}
	}
	entry.complete = true;
	const auto serial = entry.serial;
	const auto done = [=](const MTPmessages_Messages &result) {
		const auto found = _entries.find(key);
		if (found == _entries.end() || found->second.serial != serial) {
			return;
		}
		if (!found->second.thread) {
			finish(key, serial, {});
			return;
		}
		_session->data().processExistingMessages(peer->asChannel(), result);
		const auto updated = _entries.find(key);
		if (updated == _entries.end() || updated->second.serial != serial) {
			return;
		}
		auto &current = updated->second;
		current.request = 0;
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			for (const auto &message : data.vmessages().v) {
				const auto item = _session->data().message(
					peer,
					IdFromMessage(message));
				if (item && !item->isEmpty()) {
					current.candidates.push_back(item->fullId());
				}
			}
		});
		match(key, current);
	};
	const auto fail = [=] { finish(key, serial, {}); };
	const auto send = [&](auto method) {
		return _api.request(std::move(method))
			.done(done).fail(fail).handleFloodErrors().send();
	};
	if (const auto topic = thread->topicRootId()) {
		entry.request = send(MTPmessages_GetReplies(
			peer->input(),
			MTP_int(topic),
			MTP_int(entry.top.msg),
			MTP_int(0),
			MTP_int(0),
			MTP_int(kPreviewLookback),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)));
	} else {
		entry.request = send(MTPmessages_GetHistory(
			peer->input(),
			MTP_int(entry.top.msg),
			MTP_int(0),
			MTP_int(0),
			MTP_int(kPreviewLookback),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)));
	}
}

void PreviewCache::match(quintptr key, PreviewEntry &entry) {
	const auto thread = entry.thread.get();
	const auto peer = thread->peer();
	const auto top = _session->data().message(entry.top);
	if (!top) {
		finish(key, entry.serial, {});
		return;
	}
	const auto address = PeerAddress{
		.kind = peer->isUser() ? PeerKind::User
			: peer->isChat() ? PeerKind::Group : PeerKind::Channel,
		.id = peer->id.value & PeerId::kChatTypeMask,
	};
	auto candidates = std::vector<PreviewCandidate>();
	for (const auto &id : entry.candidates) {
		const auto item = _session->data().message(id);
		if (!item || item->date() > top->date()
			|| (thread->topicRootId() && item->topicRootId() != thread->topicRootId())) {
			continue;
		}
		candidates.push_back({
			.id = id.msg.bare,
			.text = item->originalText().text.toStdU16String(),
			.incoming = item->isRegular() && !item->out(),
			.senderHidden = item->isRegular() && SenderHidden(item->from()),
		});
	}
	const auto rules = Read(_session).rules;
	const auto enabled = EnabledForPeer(peer);
	const auto serial = entry.serial;
	const auto topId = entry.top;
	const auto weak = base::make_weak(this);
	crl::async([=, candidates = std::move(candidates)] {
		const auto chosen = SelectPreview(candidates, address, rules, enabled);
		crl::on_main(weak, [=] {
			finish(key, serial, chosen
				? FullMsgId(topId.peer, MsgId(chosen)) : FullMsgId());
		});
	});
}

void PreviewCache::finish(quintptr key, uint64 serial, FullMsgId selected) {
	const auto found = _entries.find(key);
	if (found == _entries.end() || found->second.serial != serial) {
		return;
	}
	auto &entry = found->second;
	if (!selected && !entry.complete && entry.thread) {
		entry.candidates.clear();
		request(key, entry);
		return;
	}
	entry.selected = selected;
	entry.ready = true;
	entry.working = false;
	--_active;
	if (const auto thread = entry.thread.get()) {
		thread->updateChatListEntry();
	}
	pump();
}

PreviewCache &ForSession(not_null<Main::Session*> session) {
	static auto caches = std::map<Main::Session*, base::weak_ptr<PreviewCache>>();
	const auto found = caches.find(session);
	if (found != caches.end() && found->second) {
		return *found->second;
	}
	std::erase_if(caches, [](const auto &pair) { return !pair.second; });
	const auto created = session->lifetime().make_state<PreviewCache>(session);
	caches.emplace(session, base::make_weak(created));
	return *created;
}

} // namespace

HistoryItem *PreviewItem(
		not_null<Data::Thread*> thread,
		not_null<HistoryItem*> top) {
	if (thread->asSublist()) {
		return Hidden(top) ? nullptr : top.get();
	}
	return ForSession(&thread->session()).get(thread, top);
}

} // namespace Regram::Filters
