#include "regram/nagram/compose/spacing.h"

#include <gsl/assert>

namespace Nagram::Compose {
namespace {

bool IsHan(char32_t ch) {
	return QChar::script(ch) == QChar::Script_Han;
}

bool IsLatinOrDigit(char32_t ch) {
	return (ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9');
}

} // namespace

SpacingResult InsertChineseLatinSpacing(
		const QString &text,
		const std::vector<int> &protectedBoundaries) {
	const auto length = int(text.size());
	Expects(protectedBoundaries.size() == length + 1);
	auto result = SpacingResult{
		.text = QString(),
		.before = std::vector<int>(length + 1),
		.after = std::vector<int>(length + 1),
	};
	result.text.reserve(length);
	auto previous = char32_t(0);
	auto depth = 0;
	for (auto i = 0; i != length;) {
		depth += protectedBoundaries[i];
		const auto first = text.at(i);
		const auto surrogate = first.isHighSurrogate()
			&& i + 1 < length
			&& text.at(i + 1).isLowSurrogate();
		const auto ch = surrogate
			? QChar::surrogateToUcs4(first, text.at(i + 1))
			: char32_t(first.unicode());
		result.before[i] = result.text.size();
		if (!depth && ((IsHan(previous) && IsLatinOrDigit(ch))
			|| (IsLatinOrDigit(previous) && IsHan(ch)))) {
			result.text += u' ';
		}
		result.after[i] = result.text.size();
		result.text += first;
		if (surrogate) {
			++i;
			depth += protectedBoundaries[i];
			result.before[i] = result.after[i] = result.text.size();
			result.text += text.at(i);
		}
		++i;
		previous = ch;
	}
	result.before[length] = result.after[length] = result.text.size();
	return result;
}

} // namespace Nagram::Compose
