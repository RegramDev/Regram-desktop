#pragma once

#include <gsl/pointers>

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram {

void ShowRestartPrompt(gsl::not_null<Window::SessionController*> controller);

} // namespace Nagram
