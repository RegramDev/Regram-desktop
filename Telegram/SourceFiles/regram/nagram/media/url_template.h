#pragma once

#include <QtCore/QString>

namespace Nagram::Media {

inline constexpr auto kMaxCoverUrlLength = 512;

[[nodiscard]] bool ValidCoverAddress(const QString &address);
[[nodiscard]] bool ValidCoverUrl(const QString &value);
[[nodiscard]] QString CoverUrlHost(const QString &address);
[[nodiscard]] QString ExpandCoverUrl(
	const QString &address,
	const QString &artist,
	const QString &title);

} // namespace Nagram::Media
