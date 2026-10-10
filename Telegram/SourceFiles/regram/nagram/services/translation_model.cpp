#include "regram/nagram/services/translation_model.h"

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "ui/text/text_utilities.h"

namespace Nagram {
namespace {

bool Protected(EntityType type) {
	switch (type) {
	case EntityType::Bold:
	case EntityType::Semibold:
	case EntityType::Italic:
	case EntityType::Underline:
	case EntityType::StrikeOut:
	case EntityType::Blockquote:
	case EntityType::Spoiler:
	case EntityType::Subscript:
	case EntityType::Superscript:
	case EntityType::Marked:
	case EntityType::Colorized:
		return false;
	default:
		return true;
	}
}

} // namespace

std::optional<TranslationPlan> PlanTranslation(TextWithEntities text) {
	const auto length = int(text.text.size());
	if (length > 1024 * 1024 || text.text.contains(QChar(0))
		|| QString::fromUtf8(text.text.toUtf8()) != text.text) {
		return std::nullopt;
	}
	auto boundaries = base::flat_set<int>{ 0, length };
	auto protection = std::vector<int>(length + 1);
	const auto collect = [&](const EntitiesInText &entities) {
		for (const auto &entity : entities) {
			if (!entity.validForText(length)) {
				return false;
			}
			const auto start = entity.offset();
			const auto end = start + entity.length();
			if ((start && text.text[start].isLowSurrogate())
				|| (end < length && text.text[end].isLowSurrogate())) {
				return false;
			}
			boundaries.emplace(start);
			boundaries.emplace(end);
			if (Protected(entity.type())) {
				++protection[start];
				--protection[end];
			}
		}
		return true;
	};
	if (!collect(text.entities)
		|| !collect(TextUtilities::ParseEntities(text.text,
			TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands).entities)) {
		return std::nullopt;
	}
	auto codeStart = -1;
	auto codeTicks = 0;
	const auto protectCode = [&](int end) {
		boundaries.emplace(codeStart);
		boundaries.emplace(end);
		++protection[codeStart];
		--protection[end];
	};
	for (auto i = 0; i < length;) {
		if (text.text[i] == '\\' && codeStart < 0) {
			i += std::min(2, length - i);
		} else if (text.text[i] != '`') {
			++i;
		} else {
			const auto start = i;
			while (i < length && text.text[i] == '`') {
				++i;
			}
			const auto ticks = i - start;
			if (codeStart < 0) {
				codeStart = start;
				codeTicks = ticks;
			} else if (ticks == codeTicks) {
				protectCode(i);
				codeStart = -1;
			}
		}
	}
	if (codeStart >= 0) {
		protectCode(length);
	}
	auto active = 0;
	for (auto &value : protection) {
		active += value;
		value = active;
	}
	auto result = TranslationPlan{ .original = std::move(text) };
	auto previous = 0;
	for (const auto &boundary : boundaries) {
		if (boundary == previous) {
			continue;
		}
		const auto part = result.original.text.mid(previous, boundary - previous);
		const auto translate = !protection[previous] && !part.trimmed().isEmpty();
		auto start = previous;
		auto end = boundary;
		if (translate) {
			while (start < end && result.original.text[start].isSpace()) {
				++start;
			}
			while (end > start && result.original.text[end - 1].isSpace()) {
				--end;
			}
		}
		if (start > previous) {
			result.parts.push_back({ previous, start - previous, -1 });
		}
		result.parts.push_back({
			.start = start,
			.length = end - start,
			.index = translate ? int(result.texts.size()) : -1,
		});
		if (translate) {
			result.texts.push_back(result.original.text.mid(start, end - start));
		}
		if (end < boundary) {
			result.parts.push_back({ end, boundary - end, -1 });
		}
		previous = boundary;
	}
	return result;
}

std::optional<TextWithEntities> ApplyTranslation(
		const TranslationPlan &plan,
		const QStringList &translated) {
	if (translated.size() != plan.texts.size()) {
		return std::nullopt;
	}
	auto boundaries = base::flat_map<int, int>();
	auto result = TextWithEntities();
	for (const auto &part : plan.parts) {
		boundaries[part.start] = result.text.size();
		const auto text = part.index < 0
			? plan.original.text.mid(part.start, part.length)
			: translated[part.index];
		if (text.isEmpty() || QString::fromUtf8(text.toUtf8()) != text
			|| text.contains(QChar(0)) || result.text.size() + text.size() > 1024 * 1024) {
			return std::nullopt;
		}
		result.text += text;
		boundaries[part.start + part.length] = result.text.size();
	}
	for (const auto &entity : plan.original.entities) {
		const auto start = boundaries.find(entity.offset());
		const auto end = boundaries.find(entity.offset() + entity.length());
		if (start == boundaries.end() || end == boundaries.end()) {
			return std::nullopt;
		}
		result.entities.push_back(EntityInText(
			entity.type(), start->second, end->second - start->second, entity.data()));
	}
	return result;
}

} // namespace Nagram
