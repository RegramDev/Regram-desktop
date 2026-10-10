#include "regram/nagram/export/range_model.h"

namespace Nagram::Export {
namespace {

[[nodiscard]] int AtOrAfter(const std::optional<Anchor> &anchor, int fallback) {
	return !anchor
		? fallback
		: !anchor->id
		? 0
		: anchor->newer
		? (*anchor->newer + 1)
		: fallback;
}

} // namespace

int RangeCount(
		int total,
		const std::optional<Anchor> &from,
		const std::optional<Anchor> &till) {
	const auto first = from ? from->id : 1;
	if (total <= 0 || !first || (till && till->id && till->id <= first)) {
		return 0;
	}
	// The server may leave the offsets out or inexact, so stay an upper bound.
	return std::clamp(
		AtOrAfter(from, total) - AtOrAfter(till, 0),
		1,
		total);
}

void ResolveRange(
		int total,
		TimeId from,
		TimeId till,
		Lookup lookup,
		Fn<void(Range)> done) {
	if (total <= 0 || (from <= 0 && till <= 0)) {
		done({ .count = std::max(total, 0) });
		return;
	}
	const auto finish = [=](
			std::optional<Anchor> start,
			std::optional<Anchor> end) {
		done({
			.firstId = (start && start->id) ? start->id : 1,
			.count = RangeCount(total, start, end),
		});
	};
	const auto resolveTill = [=](std::optional<Anchor> start) {
		if (till > 0) {
			lookup(till, [=](Anchor end) { finish(start, end); });
		} else {
			finish(start, std::nullopt);
		}
	};
	if (from > 0) {
		lookup(from, [=](Anchor start) { resolveTill(start); });
	} else {
		resolveTill(std::nullopt);
	}
}

} // namespace Nagram::Export
