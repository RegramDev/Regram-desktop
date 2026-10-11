#pragma once

#include "regram/filters/rules.h"

namespace Regram::Filters {

inline constexpr auto kPreviewLookback = 50;

struct PreviewCandidate {
	std::int64_t id = 0;
	std::u16string text;
	bool incoming = true;
	bool senderHidden = false;
};

[[nodiscard]] std::int64_t SelectPreview(
	const std::vector<PreviewCandidate> &candidates,
	std::optional<PeerAddress> peer,
	const std::vector<Rule> &rules,
	bool keywordsEnabled);

} // namespace Regram::Filters
