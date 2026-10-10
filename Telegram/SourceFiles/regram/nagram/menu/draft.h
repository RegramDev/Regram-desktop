#pragma once

#include "data/data_msg_id.h"
#include "ui/text/text_entity.h"

namespace Data {
class Thread;
} // namespace Data
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Menu {

[[nodiscard]] bool DraftOccupied(not_null<Data::Thread*> target);
void PlaceTextDraft(
	not_null<Window::SessionController*> controller,
	not_null<Data::Thread*> target,
	TextWithTags text,
	FullMsgId replyTo = FullMsgId());

} // namespace Nagram::Menu
