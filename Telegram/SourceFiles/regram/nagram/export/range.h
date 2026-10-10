#pragma once

#include "regram/nagram/export/range_model.h"
#include "scheme.h"

namespace Nagram::Export {

[[nodiscard]] inline Anchor ParseAnchor(const MTPmessages_Messages &result) {
	return result.match([](const MTPDmessages_messagesNotModified &) {
		return Anchor();
	}, [](const auto &data) {
		const auto &list = data.vmessages().v;
		auto anchor = Anchor();
		if (!list.isEmpty()) {
			anchor.id = list.front().match([](const auto &message) {
				return message.vid().v;
			});
		}
		if constexpr (!MTPDmessages_messages::Is<decltype(data)>()) {
			if (const auto offset = data.voffset_id_offset()) {
				anchor.newer = offset->v;
			}
		}
		return anchor;
	});
}

} // namespace Nagram::Export
