#pragma once

#include "base/assertion.h"
#include "ui/text/text_entity.h"

#include <QtCore/QStringList>

#include <optional>

namespace Nagram {

struct TranslationPart {
	int start = 0;
	int length = 0;
	int index = -1;
};

struct TranslationPlan {
	TextWithEntities original;
	std::vector<TranslationPart> parts;
	QStringList texts;
};

[[nodiscard]] std::optional<TranslationPlan> PlanTranslation(TextWithEntities text);
[[nodiscard]] std::optional<TextWithEntities> ApplyTranslation(
	const TranslationPlan &plan,
	const QStringList &translated);
} // namespace Nagram
