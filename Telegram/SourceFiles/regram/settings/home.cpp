#include "regram/settings/home.h"

#include "regram/nagram/settings/home.h"

namespace Regram::Settings {

::Settings::Type HomeId() {
    return Nagram::HomeId();
}

void AddEntry(::Settings::Builder::SectionBuilder &builder) {
    Nagram::AddSettingsEntry(builder);
}

} // namespace Regram::Settings
