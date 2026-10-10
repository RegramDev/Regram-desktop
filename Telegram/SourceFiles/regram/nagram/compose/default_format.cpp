#include "regram/nagram/compose/default_format.h"

namespace Nagram::Compose {
namespace {

EntityType FormatType(DefaultFormat format) {
	switch (format) {
	case DefaultFormat::Bold: return EntityType::Bold;
	case DefaultFormat::Italic: return EntityType::Italic;
	case DefaultFormat::Underline: return EntityType::Underline;
	case DefaultFormat::Strike: return EntityType::StrikeOut;
	case DefaultFormat::Spoiler: return EntityType::Spoiler;
	case DefaultFormat::None: return EntityType::Invalid;
	}
	Unexpected("Default format");
}

} // namespace

TextWithEntities ApplyDefaultFormat(TextWithEntities text, DefaultFormat format) {
	if (format == DefaultFormat::None || text.text.isEmpty()) {
		return text;
	}
	const auto length = int(text.text.size());
	auto boundaries = std::vector<int>(length + 1);
	for (const auto &entity : text.entities) {
		if (!entity.validForText(length)) {
			return text;
		}
		const auto start = entity.offset();
		const auto end = start + entity.length();
		if ((start && text.text[start].isLowSurrogate())
			|| (end < length && text.text[end].isLowSurrogate())) {
			return text;
		}
		++boundaries[entity.offset()];
		--boundaries[entity.offset() + entity.length()];
	}
	auto active = 0;
	auto start = -1;
	for (auto index = 0; index <= length; ++index) {
		active += boundaries[index];
		const auto allowed = index < length && !active;
		if (allowed && start < 0) {
			start = index;
		} else if (!allowed && start >= 0) {
			if (!text.text.mid(start, index - start).trimmed().isEmpty()) {
				text.entities.push_back(EntityInText(FormatType(format), start, index - start));
			}
			start = -1;
		}
	}
	return text;
}

} // namespace Nagram::Compose
