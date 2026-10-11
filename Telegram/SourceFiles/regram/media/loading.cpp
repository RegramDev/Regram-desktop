#include "regram/media/loading.h"

#include "base/timer.h"
#include "base/flat_set.h"
#include "base/weak_qptr.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_document.h"
#include "data/data_media_preload.h"
#include "main/main_session_settings.h"
#include "data/data_document_media.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_media_types.h"
#include "history/history_item.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "history/view/media/history_view_media.h"
#include "main/main_session.h"
#include "regram/nagram/media/options.h"

#include <QtGui/QGuiApplication>
#include <QtWidgets/QWidget>
#include <map>
#include <memory>
#include <utility>

namespace Regram::Media {
namespace {

constexpr auto kRefreshInterval = crl::time(200);
constexpr auto kMaxCandidates = 128;
constexpr auto kMaxPlaying = 2;

struct Viewport {
	base::weak_qptr<QWidget> widget;
	int top = 0;
	int bottom = 0;
	std::vector<FullMsgId> messages;
};

struct Candidate {
	base::weak_ptr<HistoryView::Element> view;
	bool admitted = false;
	crl::time retiredAt = 0;
};

struct Prefetched {
	FullMsgId context;
	const void *resource = nullptr;
	std::variant<std::shared_ptr<Data::PhotoMedia>, std::shared_ptr<Data::DocumentMedia>> media;
	crl::time expires = 0;
	std::unique_ptr<Data::VideoPreload> video;
};

struct State {
	std::map<HistoryView::ElementDelegate*, Viewport> viewports;
	std::vector<Candidate> candidates;
	std::vector<Prefetched> prefetched;
	bool prefetchDirty = true;
	base::Timer timer;
};

using States = std::map<Main::Session*, std::unique_ptr<State>>;

States &Storage() {
	static auto result = States();
	return result;
}

void RefreshPrefetch(not_null<Main::Session*> session, not_null<State*> state) {
	const auto now = crl::now();
	if (!Nagram::ForDevice().Get(Nagram::Media::kLoadingExperiment)
		|| QGuiApplication::applicationState() != Qt::ApplicationActive) {
		state->prefetched.clear();
		state->prefetchDirty = true;
		return;
	}
	std::erase_if(state->prefetched, [=](const Prefetched &entry) { return entry.expires && entry.expires <= now; });
	if (!std::exchange(state->prefetchDirty, false)) {
		return;
	}

	auto desired = std::vector<std::pair<HistoryItem*, const void*>>();
	if (Nagram::ForDevice().Get(Nagram::Media::kLoadingExperiment)
		&& QGuiApplication::applicationState() == Qt::ApplicationActive) {
		for (const auto &entry : state->viewports) {
			const auto &viewport = entry.second;
			if (!viewport.widget || !viewport.widget->isVisible()) {
				continue;
			}
			for (const auto id : viewport.messages) {
				const auto item = session->data().message(id);
				const auto media = item ? item->media() : nullptr;
				if (!media || desired.size() >= 12) {
					continue;
				}
				const auto resource = media->photo() ? static_cast<const void*>(media->photo())
					: static_cast<const void*>(media->document());
				if (resource && !ranges::any_of(desired, [&](const auto &entry) { return entry.second == resource; })) {
					desired.emplace_back(item, resource);
				}
			}
		}
	}
	for (auto &entry : state->prefetched) {
		if (ranges::any_of(desired, [&](const auto &item) { return item.second == entry.resource; })) {
			entry.expires = 0;
		} else if (!entry.expires) {
			entry.expires = now + crl::time(1500);
		}
	}
	std::erase_if(state->prefetched, [=](const Prefetched &entry) { return entry.expires && entry.expires <= now; });
	auto retired = 0;
	std::erase_if(state->prefetched, [&](const Prefetched &entry) { return entry.expires && ++retired > 3; });
	for (const auto &[item, resource] : desired) {
		const auto id = item->fullId();
		const auto found = ranges::find(state->prefetched, resource, &Prefetched::resource);
		if (found != state->prefetched.end()) {
			continue;
		}
		if (const auto photo = item->media()->photo()) {
			auto media = photo->createMediaView();
			media->automaticLoad(id, item);
			state->prefetched.push_back({ id, resource, std::move(media) });
		} else if (const auto document = item->media()->document()) {
			auto media = document->createMediaView();
			media->thumbnailWanted(id);
			if (!document->canBeStreamed()) {
				media->automaticLoad(id, item);
			}
			auto entry = Prefetched{ id, resource, std::move(media) };
			if (Data::VideoPreload::Can(document) && Data::AutoDownload::ShouldAutoPlay(
					session->settings().autoDownload(), item->history()->peer, document)) {
				entry.video = std::make_unique<Data::VideoPreload>(document, id, [] {});
			}
			state->prefetched.push_back(std::move(entry));
		}
	}
}

void Refresh(not_null<Main::Session*> session, not_null<State*> state) {
	std::erase_if(state->viewports, [](const auto &entry) { return !entry.second.widget; });
	std::erase_if(state->candidates, [](const Candidate &entry) { return !entry.view; });
	auto ranked = std::vector<std::pair<int, HistoryView::Element*>>();
	auto visible = base::flat_set<HistoryView::Element*>();
	const auto enabled = Nagram::ForDevice().Get(Nagram::Media::kLoadingExperiment);
	if (enabled && QGuiApplication::applicationState() == Qt::ApplicationActive) {
		for (const auto &entry : state->candidates) {
			const auto view = entry.view.get();
			const auto viewport = state->viewports.find(view->delegate());
			if (viewport == state->viewports.end() || !viewport->second.widget->isVisible()) {
				continue;
			}
			const auto top = viewport->second.top;
			const auto bottom = viewport->second.bottom;
			if (top >= bottom || view->delegate()->elementAnimationsPaused()
				|| !view->delegate()->elementIntersectsRange(view, top, bottom)) {
				continue;
			}
			visible.emplace(view);
			const auto quarter = (bottom - top) / 4;
			const auto centered = view->delegate()->elementIntersectsRange(view, top + quarter, bottom - quarter);
			ranked.emplace_back(centered ? 0 : 1, view);
		}
		std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		if (ranked.size() > kMaxPlaying) {
			ranked.resize(kMaxPlaying);
		}
	}
	auto notify = std::vector<std::pair<base::weak_ptr<HistoryView::Element>, bool>>();
	for (auto &entry : state->candidates) {
		entry.retiredAt = visible.contains(entry.view.get()) ? 0
			: entry.retiredAt ? entry.retiredAt : crl::now();
		const auto admitted = !enabled || ranges::any_of(ranked, [&](const auto &chosen) { return chosen.second == entry.view.get(); });
		if (entry.admitted != admitted || !admitted) {
			const auto changed = entry.admitted != admitted;
			entry.admitted = admitted;
			notify.emplace_back(entry.view, changed);
		}
	}
	for (const auto &[weak, changed] : notify) {
		if (const auto view = weak.get()) {
			if (const auto media = view->media()) {
				media->checkAnimation();
			}
			if (changed) {
				session->data().requestViewRepaint(view);
			}
		}
	}
	std::erase_if(state->candidates, [](const Candidate &entry) {
		return entry.retiredAt && crl::now() - entry.retiredAt >= crl::time(1500);
	});
	RefreshPrefetch(session, state);
	if (enabled && (!state->candidates.empty()
		|| ranges::any_of(state->prefetched, [](const Prefetched &entry) { return entry.expires != 0; }))) {
		state->timer.callOnce(kRefreshInterval);
	}
}

State &ForSession(not_null<Main::Session*> session) {
	auto &entry = Storage()[session];
	if (!entry) {
		entry = std::make_unique<State>();
		const auto state = entry.get();
		state->timer.setCallback([=] { Refresh(session, state); });
		session->lifetime().add([session] { Storage().erase(session); });
		Nagram::ForDevice().Value(Nagram::Media::kLoadingExperiment)
			| rpl::on_next([=] { state->prefetchDirty = true; state->timer.callOnce(0); }, session->lifetime());
		Core::App().appDeactivatedValue() | rpl::on_next([=] {
			state->prefetchDirty = true;
			state->timer.callOnce(0);
		}, session->lifetime());
	}
	return *entry;
}

} // namespace

void UpdateViewport(
		not_null<Main::Session*> session,
		not_null<HistoryView::ElementDelegate*> delegate,
		not_null<QWidget*> widget,
		int top,
		int bottom) {
	auto &state = ForSession(session);
	auto &viewport = state.viewports[delegate];
	viewport.widget = base::make_weak(widget.get());
	viewport.top = top;
	viewport.bottom = bottom;
	state.prefetchDirty = true;
	if (Nagram::ForDevice().Get(Nagram::Media::kLoadingExperiment)) {
		state.timer.callOnce(0);
	}
}

void PrefetchWindow(
		not_null<Main::Session*> session,
		not_null<HistoryView::ElementDelegate*> delegate,
		std::vector<FullMsgId> messages) {
	auto &state = ForSession(session);
	state.viewports[delegate].messages = std::move(messages);
	state.prefetchDirty = true;
	state.timer.callOnce(0);
}

bool AdmitInline(not_null<HistoryView::Element*> view) {
	if (!Nagram::ForDevice().Get(Nagram::Media::kLoadingExperiment)) {
		return true;
	}
	auto &state = ForSession(&view->history()->session());
	if (!state.viewports.contains(view->delegate())) {
		return true;
	}
	const auto found = ranges::find_if(state.candidates, [&](const Candidate &entry) { return entry.view.get() == view; });
	if (found != state.candidates.end()) {
		return found->admitted;
	}
	std::erase_if(state.candidates, [](const Candidate &entry) { return !entry.view; });
	if (state.candidates.size() >= kMaxCandidates) {
		return false;
	}
	state.candidates.push_back({ base::make_weak(view.get()) });
	state.timer.callOnce(0);
	return false;
}

void ResetPrefetch(not_null<Main::Session*> session) {
	const auto found = Storage().find(session);
	if (found != Storage().end()) {
		found->second->prefetched.clear();
		found->second->prefetchDirty = true;
		found->second->timer.callOnce(0);
	}
}

void ForgetInline(not_null<HistoryView::Element*> view) {
	const auto found = Storage().find(&view->history()->session());
	if (found == Storage().end()) {
		return;
	}
	auto &state = *found->second;
	std::erase_if(state.candidates, [&](const Candidate &entry) { return !entry.view || entry.view.get() == view; });
	state.timer.callOnce(0);
}

} // namespace Regram::Media
