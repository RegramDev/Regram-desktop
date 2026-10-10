#include "regram/nagram/links/behavior.h"

#include "regram/nagram/links/options.h"
#include "regram/nagram/links/webview.h"
#include "core/file_utilities.h"
#include "core/click_handler_types.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "dialogs/dialogs_key.h"
#include "dialogs/ui/chat_search_in.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

namespace Nagram::Links {
namespace {

constexpr auto kWebAppScreenMargin = 80;

auto ClickActive = false;
PeerData *ClickPeer = nullptr;

[[nodiscard]] PeerData *PeerFromContext(const ClickContext &context) {
	const auto my = context.other.value<ClickHandlerContext>();
	if (my.peer) {
		return my.peer;
	}
	const auto controller = my.sessionWindow.get();
	const auto item = (controller && my.itemId)
		? controller->session().data().message(my.itemId)
		: nullptr;
	return item ? item->history()->peer.get() : nullptr;
}

} // namespace

bool AutoLoginDisabled() {
	return ForDevice().Get(kDisableOfficialAutoLogin);
}

HashtagClickScope::HashtagClickScope(
	const ClickContext &context,
	const QString &tag)
: _wasActive(ClickActive)
, _wasPeer(ClickPeer) {
	ClickActive = !tag.contains(u'@');
	ClickPeer = ClickActive ? PeerFromContext(context) : nullptr;
}

HashtagClickScope::~HashtagClickScope() {
	ClickActive = _wasActive;
	ClickPeer = _wasPeer;
}

void ApplyHashtagSearchPage(Dialogs::SearchState &state) {
	const auto inChat = state.inChat.peer();
	const auto peer = inChat ? inChat : ClickPeer;
	if (!peer || !state.tags.empty()) {
		return;
	}
	const auto page = ResolveHashtagPage(
		ClickActive,
		peer->isBroadcast(),
		ForDevice().Get(kHashtagSearchPageChannel),
		ForDevice().Get(kHashtagSearchPageChat));
	if (page == HashtagPage::MyMessages) {
		state.inChat = Dialogs::Key();
		state.fromPeer = nullptr;
		state.tab = Dialogs::ChatSearchTab::MyMessages;
	} else if (page == HashtagPage::ThisChat && !inChat) {
		state.inChat = peer->owner().history(peer).get();
		state.tab = state.defaultTabForMe();
	}
}

QSize WebAppPanelSize(QSize base) {
	const auto width = ForDevice().Get(kWebAppWidthScale);
	const auto height = ForDevice().Get(kWebAppHeightScale);
	if (width == 100 && height == 100) {
		return base;
	}
	const auto window = QGuiApplication::focusWindow();
	const auto screen = window
		? window->screen()
		: QGuiApplication::primaryScreen();
	const auto margin = style::ConvertScale(kWebAppScreenMargin);
	const auto available = screen
		? (screen->availableGeometry().size() - QSize(margin, margin))
		: QSize();
	return ScaledPanelSize(base, width, height, available);
}

const char *WebAppPlatform(not_null<UserData*> bot) {
	return WebAppPlatformName(ForDevice().Get(kWebAppAndroidPlatform));
}

bool OpenOutsideWebview(const QString &uri, const QString &startUrl) {
	const auto pattern = ForDevice().Get(kWebviewExternalPattern);
	if (pattern.isEmpty()) {
		return false;
	}
	static auto compiledFor = QString();
	static auto compiled = QRegularExpression();
	static auto lastOpen = qint64();
	if (compiledFor != pattern) {
		compiledFor = pattern;
		compiled = CompileWebviewPattern(pattern);
	}
	const auto decision = MatchOutsideWebview(compiled, uri, startUrl);
	if (!decision.error.isEmpty()) {
		LOG(("Nagram web app link: %1.").arg(decision.error));
	} else if (decision.open && !AllowOutsideOpen(lastOpen, crl::now())) {
		LOG(("Nagram web app link: more than one per second, kept inside."));
		return false;
	}
	if (decision.open) {
		File::OpenUrl(uri);
	}
	return decision.open;
}

} // namespace Nagram::Links
