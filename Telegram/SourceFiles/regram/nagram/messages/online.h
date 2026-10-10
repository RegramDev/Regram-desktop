#pragma once

class QPainter;
class PeerData;
namespace Main {
class Session;
} // namespace Main
namespace Ui {
class RpWidget;
} // namespace Ui

namespace Nagram::Messages {

void PaintSenderOnline(
	QPainter &p,
	not_null<PeerData*> peer,
	int x,
	int y,
	int size);
void RepaintOnSenderOnline(
	not_null<Ui::RpWidget*> widget,
	not_null<Main::Session*> session);

} // namespace Nagram::Messages
