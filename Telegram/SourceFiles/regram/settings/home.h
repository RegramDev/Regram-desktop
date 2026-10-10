#pragma once

#include "settings/settings_type.h"

namespace Settings::Builder {
class SectionBuilder;
} // namespace Settings::Builder

namespace Regram::Settings {

[[nodiscard]] ::Settings::Type HomeId();
void AddEntry(::Settings::Builder::SectionBuilder &builder);

} // namespace Regram::Settings
