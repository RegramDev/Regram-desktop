#include "regram/filters/preview_policy.h"

namespace Regram::Filters {

std::int64_t SelectPreview(
		const std::vector<PreviewCandidate> &candidates,
		std::optional<PeerAddress> peer,
		const std::vector<Rule> &rules,
		bool keywordsEnabled) {
	auto engine = Engine();
	const auto valid = engine.setRules(rules);
	auto examined = 0;
	for (const auto &candidate : candidates) {
		if (++examined > kPreviewLookback) {
			break;
		}
		if (candidate.senderHidden) {
			continue;
		}
		if (!keywordsEnabled || !candidate.incoming || !valid
			|| !engine.evaluate(candidate.text, peer).hidden) {
			return candidate.id;
		}
	}
	return 0;
}

} // namespace Regram::Filters
