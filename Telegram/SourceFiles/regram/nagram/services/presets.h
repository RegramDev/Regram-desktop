#pragma once

#include "regram/nagram/services/model.h"

#include <span>

namespace Nagram {

struct ServicePreset {
	const char *id = nullptr;
	const char16_t *name = nullptr;
	ServiceKind kind = ServiceKind::Translation;
	const char16_t *protocol = nullptr;
	const char16_t *baseUrl = nullptr;
	const char16_t *endpoint = nullptr;
};

[[nodiscard]] std::span<const ServicePreset> ServicePresets();
[[nodiscard]] ServiceDefinition ServiceFromPreset(
	const ServicePreset &preset,
	const QString &id,
	const QString &credentialRef);

} // namespace Nagram
