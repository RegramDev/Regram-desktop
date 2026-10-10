#pragma once

#include "ui/text/text_entity.h"

#include <optional>

namespace Nagram::Messages {

[[nodiscard]] constexpr bool ChineseConversionAvailable() {
#if defined Q_OS_MAC || defined Q_OS_WIN
	return true;
#else
	return false;
#endif
}

[[nodiscard]] std::optional<TextWithEntities> ConvertChinese(
	const TextWithEntities &source,
	bool traditional);
[[nodiscard]] std::optional<TextWithEntities> ProjectReading(
	const TextWithEntities &source,
	bool spacing,
	int chinese);

class ReadingCache final {
public:
	[[nodiscard]] const TextWithEntities &Get(
		const TextWithEntities &source,
		bool spacing,
		int chinese);

private:
	TextWithEntities _source;
	TextWithEntities _display;
	bool _spacing = false;
	int _chinese = 0;
	bool _valid = false;
};

} // namespace Nagram::Messages
