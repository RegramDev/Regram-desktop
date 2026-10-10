#pragma once

#include <unicode/regex.h>

namespace Regram::Filters {

enum class Match { None, Found, Limited };

[[nodiscard]] Match MatchPattern(
	const icu::RegexPattern &pattern,
	const icu::UnicodeString &text);

} // namespace Regram::Filters
