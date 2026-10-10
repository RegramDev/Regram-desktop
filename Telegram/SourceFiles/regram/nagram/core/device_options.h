#pragma once

#include "regram/nagram/core/options.h"

namespace Nagram::details {

[[nodiscard]] inline Options &SharedDeviceOptions(RawPrefs &prefs) {
	static auto options = Options(prefs);
	return options;
}

} // namespace Nagram::details
