#include "regram/filters/matcher.h"

#include "regram/filters/rules.h"

#include <memory>

namespace Regram::Filters {
namespace {

constexpr auto kMatchTimeLimit = 5;
constexpr auto kStackLimit = 1024 * 1024;

} // namespace

Match MatchPattern(const icu::RegexPattern &pattern, const icu::UnicodeString &text) {
	if (text.length() > kMaxTextUnits) {
		return Match::Limited;
	}
	auto status = U_ZERO_ERROR;
	const auto matcher = std::unique_ptr<icu::RegexMatcher>(pattern.matcher(text, status));
	if (U_FAILURE(status) || !matcher) {
		return Match::Limited;
	}
	matcher->setTimeLimit(kMatchTimeLimit, status);
	matcher->setStackLimit(kStackLimit, status);
	const auto found = matcher->find(status);
	return U_FAILURE(status) ? Match::Limited : found ? Match::Found : Match::None;
}

} // namespace Regram::Filters
