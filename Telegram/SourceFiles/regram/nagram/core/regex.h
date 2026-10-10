#pragma once

#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#include <optional>

namespace Nagram::Regex {

enum class Found { No, Yes, Limit };

struct Problem {
	int position = 0;
	QString text;
};

[[nodiscard]] QRegularExpression Compile(
	const QString &pattern,
	QRegularExpression::PatternOptions options = {});
[[nodiscard]] std::optional<Problem> Check(
	const QString &pattern,
	QRegularExpression::PatternOptions options = {});
[[nodiscard]] Found Find(
	const QRegularExpression &expression,
	const QString &text);

} // namespace Nagram::Regex
