#include "regram/nagram/privacy/protection.h"

#include "regram/nagram/display/view_refresher.h"
#include "regram/nagram/privacy/options.h"
#include "api/api_sensitive_content.h"
#include "apiwrap.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "history/history_item.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"

namespace Nagram::Privacy {
namespace {

[[nodiscard]] bool SensitiveAdjustable(not_null<Main::Session*> session) {
	const auto &content = session->api().sensitiveContent();
	return SensitiveWarningSkipped(
		true,
		content.loaded(),
		content.canChangeCurrent(),
		session->appConfig().ageVerifyNeeded());
}

[[nodiscard]] rpl::producer<bool> SensitiveAdjustableValue(
		not_null<Main::Session*> session) {
	const auto &content = session->api().sensitiveContent();
	return rpl::combine(
		content.loadedValue(),
		content.canChange(),
		rpl::single(rpl::empty) | rpl::then(session->appConfig().refreshed())
	) | rpl::map([=] {
		return SensitiveAdjustable(session);
	}) | rpl::distinct_until_changed();
}

} // namespace

bool DoNotSharePhoneByDefault() {
	return ForDevice().Get(kDoNotSharePhone);
}

bool ForceCopy() {
	return ForDevice().Get(kForceCopy);
}

rpl::producer<bool> ForceCopyChanges() {
	return ForDevice().Value(kForceCopy) | rpl::skip(1);
}

bool AllowsCopy(not_null<const PeerData*> peer) {
	return CopyAllowed(ForceCopy(), peer->allowsForwarding());
}

bool ForbidsCopy(not_null<const HistoryItem*> item) {
	return CopyForbidden(ForceCopy(), item->forbidsForward());
}

rpl::producer<bool> AllowsCopyValue(not_null<PeerData*> peer) {
	return rpl::combine(
		ForDevice().Value(kForceCopy),
		Data::AllowsForwardingValue(peer)
	) | rpl::map(CopyAllowed) | rpl::distinct_until_changed();
}

bool IgnoreRestrictions() {
	return ForDevice().Get(kIgnoreContentRestrictions);
}

void WatchRestrictions(not_null<Window::SessionController*> controller) {
	ForDevice().Value(
		kIgnoreContentRestrictions
	) | rpl::skip(1) | rpl::on_next([=] {
		if (const auto peer = controller->activeChatCurrent().peer()) {
			peer->session().changes().peerUpdated(
				peer,
				Data::PeerUpdate::Flag::UnavailableReason);
		}
	}, controller->lifetime());
}

bool SkipSensitiveWarning(not_null<Main::Session*> session) {
	return ForDevice().Get(kSkipSensitiveWarning)
		&& SensitiveAdjustable(session);
}

void AttachSensitive(not_null<Main::Session*> session) {
	SensitiveAdjustableValue(
		session
	) | rpl::skip(1) | rpl::filter([] {
		return ForDevice().Get(kSkipSensitiveWarning);
	}) | rpl::on_next([=] {
		ViewRefresher::Refresh(session->data());
	}, session->lifetime());
}

rpl::producer<> SensitiveRevealed(not_null<Main::Session*> session) {
	return rpl::combine(
		ForDevice().Value(kSkipSensitiveWarning),
		SensitiveAdjustableValue(session),
		rpl::mappers::_1 && rpl::mappers::_2
	) | rpl::distinct_until_changed(
	) | rpl::skip(1) | rpl::filter(rpl::mappers::_1) | rpl::to_empty;
}

} // namespace Nagram::Privacy
