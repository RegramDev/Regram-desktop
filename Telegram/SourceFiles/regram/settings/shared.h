#pragma once

#include <rpl/lifetime.h>

namespace Regram::Settings {

void AttachSharedSettings(rpl::lifetime &lifetime);

} // namespace Regram::Settings
