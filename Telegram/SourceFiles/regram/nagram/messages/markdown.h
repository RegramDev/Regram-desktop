#pragma once

#include <QtCore/QString>

#include <vector>

namespace Nagram::Markdown {

enum class Style {
	Bold,
	Italic,
	Underline,
	Strike,
	Spoiler,
	Link,
	Code,
	Verbatim,
	Pre,
	Quote,
};

struct Span {
	Style style = Style::Verbatim;
	int from = 0;
	int length = 0;
	QString data;
};

[[nodiscard]] QString LinkTarget(const QString &url);
[[nodiscard]] QString Convert(
	const QString &text,
	const std::vector<Span> &spans);

} // namespace Nagram::Markdown
