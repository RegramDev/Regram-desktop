#pragma once

#include "base/basic_types.h"

#include <algorithm>
#include <optional>

namespace Nagram::Export {

// The first message at or after a date; id 0 when the chat has none.
struct Anchor {
	int id = 0;
	std::optional<int> newer;
};

struct Range {
	int firstId = 1;
	int count = 0;
};

using Lookup = Fn<void(TimeId, Fn<void(Anchor)>)>;

[[nodiscard]] int RangeCount(
	int total,
	const std::optional<Anchor> &from,
	const std::optional<Anchor> &till);
void ResolveRange(
	int total,
	TimeId from,
	TimeId till,
	Lookup lookup,
	Fn<void(Range)> done);

template <typename Messages>
[[nodiscard]] bool PastRange(const Messages &list, TimeId till) {
	return (till > 0)
		&& !list.empty()
		&& std::all_of(list.begin(), list.end(), [&](const auto &message) {
			return message.date >= till;
		});
}

} // namespace Nagram::Export
