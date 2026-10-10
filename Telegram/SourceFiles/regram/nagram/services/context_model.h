#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <vector>

namespace Nagram {

inline constexpr auto kContextMessages = 6;
inline constexpr auto kContextEach = 500;
inline constexpr auto kContextTotal = 2000;

struct ContextCandidate {
	QString text;
	qint64 topic = 0;
	bool regular = true;
	bool hidden = false;
	bool restricted = false;
};

[[nodiscard]] QString TruncateContext(const QString &text, int limit);
[[nodiscard]] QStringList SelectContext(
	const std::vector<ContextCandidate> &before,
	qint64 topic,
	bool restricted);

} // namespace Nagram
