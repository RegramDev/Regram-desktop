#pragma once

#include "regram/nagram/core/exchange.h"
#include "settings/settings_common.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram {

[[nodiscard]] Settings::Type ConfigId();
void ShowImport(
	not_null<Window::SessionController*> controller,
	const QByteArray &bytes,
	ExchangeTarget target = ExchangeTarget::File,
	Fn<void()> applied = nullptr);

} // namespace Nagram
