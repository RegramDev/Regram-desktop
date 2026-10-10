#include "regram/nagram/display/view_refresher.h"

#include "regram/filters/runtime.h"
#include "regram/nagram/core/options.h"
#include "base/weak_ptr.h"
#include "crl/crl_on_main.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "main/main_session.h"

#include <vector>

namespace Nagram {

void ViewRefresher::Attach(not_null<Main::Session*> session) {
	Regram::Filters::Migrate(session);
	const auto pending = session->lifetime().make_state<bool>(false);
	const auto weak = base::make_weak(session);
	rpl::merge(
		ForDevice().changes(),
		ForAccount(session).changes()
	) | rpl::filter([](std::string_view key) {
		return RegisteredOptions().HasFlag(key, Flag::RefreshMessageView);
	}
	) | rpl::on_next([=](std::string_view) {
		if (std::exchange(*pending, true)) {
			return;
		}
		crl::on_main([=] {
			if (const auto strong = weak.get()) {
				*pending = false;
				Refresh(strong->data());
			}
		});
	}, session->lifetime());
}

void ViewRefresher::Refresh(Data::Session &data) {
	auto ids = std::vector<FullMsgId>();
	for (const auto &peer : data._messages) {
		for (const auto &message : peer.second) {
			if (message.second->mainView()) {
				ids.push_back(message.second->fullId());
			}
		}
	}
	for (const auto &id : ids) {
		if (const auto item = data.message(id); item && item->mainView()) {
			data.requestItemViewRefresh(item);
			if (const auto refreshed = data.message(id);
				refreshed && refreshed->mainView()) {
				data.requestItemResize(refreshed);
			}
		}
	}
}

} // namespace Nagram
