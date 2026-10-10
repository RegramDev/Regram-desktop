#include "regram/nagram/messages/online.h"

#include "regram/nagram/messages/options.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "styles/style_dialogs.h"

namespace Nagram::Messages {
namespace {

// 与安卓端一致，按离开在线状态的时长分级
QColor RecentlyColor(TimeId diff) {
	if (diff > -15 * 60) {
		return QColor(0xEA, 0xEA, 0x1E);
	} else if (diff > -30 * 60) {
		return QColor(0xEA, 0x84, 0x1E);
	} else if (diff > -60 * 60) {
		return QColor(0xEA, 0x1E, 0x1E);
	}
	return st::windowSubTextFg->c;
}

std::optional<QColor> ComputeBadgeColor(not_null<PeerData*> peer) {
	const auto mode = ForDevice().Get(kSenderOnlineStatus);
	const auto user = peer->asUser();
	if (!mode || !user || user->isSelf() || user->isBot()
		|| user->isServiceUser()) {
		return std::nullopt;
	}
	const auto now = base::unixtime::now();
	const auto lastseen = user->lastseen();
	if (lastseen.isOnline(now)) {
		return st::dialogsOnlineBadgeFg->c;
	} else if (mode != 2) {
		return std::nullopt;
	}
	const auto till = lastseen.onlineTill();
	if (till) {
		const auto diff = till - now;
		return (diff > -60 * 60)
			? std::optional<QColor>(RecentlyColor(diff))
			: std::nullopt;
	}
	return lastseen.isRecently()
		? std::optional<QColor>(st::windowSubTextFg->c)
		: std::nullopt;
}

} // namespace

void PaintSenderOnline(
		QPainter &p,
		not_null<PeerData*> peer,
		int x,
		int y,
		int size) {
	const auto color = ComputeBadgeColor(peer);
	if (!color) {
		return;
	}
	const auto badge = st::dialogsOnlineBadgeSize;
	const auto stroke = st::dialogsOnlineBadgeStroke;
	const auto skip = st::dialogsOnlineBadgeSkip;
	auto hq = PainterHighQualityEnabler(p);
	auto pen = QPen(st::windowBg);
	pen.setWidthF(stroke);
	p.setPen(pen);
	p.setBrush(*color);
	p.drawEllipse(QRectF(
		x + size - skip.x() - badge,
		y + size - skip.y() - badge,
		badge,
		badge));
}

void RepaintOnSenderOnline(
		not_null<Ui::RpWidget*> widget,
		not_null<Main::Session*> session) {
	session->changes().peerUpdates(
		Data::PeerUpdate::Flag::OnlineStatus
	) | rpl::filter([=](const Data::PeerUpdate &update) {
		return update.peer->isUser()
			&& widget->isVisible()
			&& ForDevice().Get(kSenderOnlineStatus) != 0;
	}) | rpl::on_next([=] {
		widget->update();
	}, widget->lifetime());
}

} // namespace Nagram::Messages
