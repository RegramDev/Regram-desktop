#include "regram/filters/reveal.h"

#include "regram/filters/runtime.h"

#include "base/timer.h"
#include "base/weak_ptr.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "regram/filters/reveal_policy.h"
#include "regram/nagram/display/view_refresher.h"

#include <map>

namespace Regram::Filters {
namespace {

struct State {
	base::weak_ptr<Main::Session> session;
	RevealPolicy policy;
	base::Timer timer;
};

void Refresh(not_null<State*> state) {
	if (const auto session = state->session.get()) {
		Nagram::ViewRefresher::Refresh(session->data());
		NotifyChanged(session);
	}
}

void Schedule(not_null<State*> state) {
	if (const auto next = state->policy.nextExpiration()) {
		state->timer.callOnce(std::max(crl::time(1), *next - crl::now()));
	} else {
		state->timer.cancel();
	}
}

State &ForPeer(not_null<PeerData*> peer) {
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	const auto session = &peer->session();
	auto found = states.find(session);
	if (found == states.end() || !found->second->session) {
		std::erase_if(states, [](const auto &entry) { return !entry.second->session; });
		auto state = std::make_unique<State>();
		state->session = base::make_weak(session);
		const auto raw = state.get();
		state->timer.setCallback([raw] {
			const auto expired = raw->policy.expire(crl::now());
			if (!expired.empty()) {
				Refresh(raw);
			}
			Schedule(raw);
		});
		found = states.emplace(session, std::move(state)).first;
	}
	return *found->second;
}

} // namespace

bool TemporarilyRevealed(not_null<PeerData*> peer) {
	return ForPeer(peer).policy.active(SerializePeerId(peer->id), crl::now());
}

bool ToggleTemporaryReveal(not_null<PeerData*> peer) {
	auto &state = ForPeer(peer);
	if (!state.policy.toggle(SerializePeerId(peer->id), crl::now())) {
		return false;
	}
	Schedule(&state);
	Refresh(&state);
	return true;
}

} // namespace Regram::Filters
