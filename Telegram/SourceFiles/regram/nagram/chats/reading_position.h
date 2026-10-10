#pragma once

#include "data/data_msg_id.h"

class History;
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Chats {

void WatchReadingPositions(not_null<Window::SessionController*> controller);
[[nodiscard]] MsgId RestoreReadingPosition(
	not_null<History*> history,
	MsgId showAtMsgId);

} // namespace Nagram::Chats
