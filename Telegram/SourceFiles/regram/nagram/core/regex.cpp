#include "regram/nagram/core/regex.h"

#include <algorithm>

namespace Nagram::Regex {
namespace {

[[nodiscard]] QString LimitPrefix() {
	return QStringLiteral(
		"(*NO_JIT)(*LIMIT_MATCH=10000)(*LIMIT_DEPTH=64)(*LIMIT_HEAP=1024)");
}

// PCRE2 reads "&&" in a class as two literals, Java reads an intersection.
[[nodiscard]] int ClassIntersection(const QString &pattern) {
	auto inClass = false;
	auto classStart = 0;
	for (auto i = 0; i < pattern.size(); ++i) {
		const auto ch = pattern[i];
		if (ch == u'\\') {
			++i;
		} else if (!inClass) {
			if (ch == u'[') {
				inClass = true;
				classStart = i + 1;
				if (classStart < pattern.size()
					&& pattern[classStart] == u'^') {
					++classStart;
				}
			}
		} else if (ch == u']' && i > classStart) {
			inClass = false;
		} else if (ch == u'&'
			&& i + 1 < pattern.size()
			&& pattern[i + 1] == u'&') {
			return i;
		}
	}
	return -1;
}

} // namespace

QRegularExpression Compile(
		const QString &pattern,
		QRegularExpression::PatternOptions options) {
	return QRegularExpression(
		LimitPrefix() + pattern,
		options | QRegularExpression::UseUnicodePropertiesOption);
}

std::optional<Problem> Check(
		const QString &pattern,
		QRegularExpression::PatternOptions options) {
	const auto expression = Compile(pattern, options);
	if (!expression.isValid()) {
		const auto offset = int(expression.patternErrorOffset())
			- int(LimitPrefix().size());
		return Problem{ std::max(offset, 0), expression.errorString() };
	} else if (const auto at = ClassIntersection(pattern); at >= 0) {
		return Problem{
			at,
			QStringLiteral("character class intersection is not supported"),
		};
	}
	return std::nullopt;
}

Found Find(const QRegularExpression &expression, const QString &text) {
	const auto match = expression.match(text);
	return !match.isValid()
		? Found::Limit
		: match.hasMatch()
		? Found::Yes
		: Found::No;
}

} // namespace Nagram::Regex
