#pragma once

#include <QtCore/QString>

namespace Nagram::Interface {

[[nodiscard]] QString HalfwidthPunctuation(QString value);
[[nodiscard]] QString UiText(QString value);
void StartUiText();

} // namespace Nagram::Interface
