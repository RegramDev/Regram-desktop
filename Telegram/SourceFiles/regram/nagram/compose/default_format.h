#pragma once

#include "base/assertion.h"
#include "ui/text/text_entity.h"

namespace Nagram::Compose {

enum class DefaultFormat { None, Bold, Italic, Underline, Strike, Spoiler };

[[nodiscard]] TextWithEntities ApplyDefaultFormat(
	TextWithEntities text,
	DefaultFormat format);

} // namespace Nagram::Compose
