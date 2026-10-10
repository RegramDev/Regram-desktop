#pragma once

#include <QtCore/QString>

#include <vector>

namespace Nagram::Compose {

struct SpacingResult {
	QString text;
	std::vector<int> before;
	std::vector<int> after;
};

[[nodiscard]] SpacingResult InsertChineseLatinSpacing(
	const QString &text,
	const std::vector<int> &protectedBoundaries);

} // namespace Nagram::Compose
