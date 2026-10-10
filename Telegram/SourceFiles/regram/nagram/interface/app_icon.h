#pragma once

#include <rpl/lifetime.h>

class QIcon;
class QImage;

namespace Nagram::Interface {

struct AppIconChoice {
	QString id;
	QString title;
};

[[nodiscard]] std::vector<AppIconChoice> AppIconChoices();
[[nodiscard]] QString AppIconTitle(const QString &id);
[[nodiscard]] QImage AppIconPreview(const QString &id);
[[nodiscard]] const QImage *CustomLogo();
[[nodiscard]] QIcon CustomAppIcon();
[[nodiscard]] int AppIconGeneration();
[[nodiscard]] QRectF TrayUnreadDot(QSize size);
void StartAppIcon(rpl::lifetime &lifetime);

} // namespace Nagram::Interface
