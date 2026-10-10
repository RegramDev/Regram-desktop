#include "regram/nagram/interface/app_icon.h"

#include "regram/nagram/interface/options.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/ayu/ui/ayu_logo.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "mainwindow.h"
#include "platform/platform_specific.h"
#include "tray.h"
#include "base/platform/base_platform_info.h"
#include "ui/ui_utility.h"
#include "window/main_window.h"
#include "window/window_controller.h"

#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <cmath>

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif // Q_OS_MAC

namespace Nagram::Interface {
namespace {

constexpr auto kLogoSize = 256;
constexpr auto kMacCanvasSize = 1024;
constexpr auto kMacBodySize = 824;

struct Loaded {
	QString id;
	QImage light;
	QImage dark;
	QImage logo;
};

auto Generation = 0;

[[nodiscard]] QString Title(QStringView id) {
	return (id == u"block") ? tr::lng_nagram_app_icon_block(tr::now)
		: (id == u"block_black")
		? tr::lng_nagram_app_icon_block_black(tr::now)
		: (id == u"block_blue")
		? tr::lng_nagram_app_icon_block_blue(tr::now)
		: (id == u"block_niello")
		? tr::lng_nagram_app_icon_block_niello(tr::now)
		: (id == u"block_purple")
		? tr::lng_nagram_app_icon_block_purple(tr::now)
		: (id == u"classic") ? tr::lng_nagram_app_icon_classic(tr::now)
		: (id == u"colorful") ? tr::lng_nagram_app_icon_colorful(tr::now)
		: (id == u"cyan") ? tr::lng_nagram_app_icon_cyan(tr::now)
		: (id == u"black") ? tr::lng_nagram_app_icon_black(tr::now)
		: tr::lng_nagram_app_icon_default(tr::now);
}

[[nodiscard]] QImage Load(const QString &id, bool dark) {
	auto image = QImage(u":/nagram/icons/block.png"_q)
		.convertToFormat(QImage::Format_ARGB32);
	if (id.isEmpty()) {
		return image;
	}
	const auto monochrome = (id == u"block_black"_q
		|| id == u"black"_q || id == u"nothing"_q);
	const auto niello = (id == u"block_niello"_q);
	const auto colorful = (id == u"colorful"_q || id == u"bard"_q);
	const auto hue = (id == u"block_blue"_q || id == u"classic"_q) ? 0.58
		: (id == u"block_purple"_q || id == u"alt"_q) ? 0.76
		: (id == u"discord"_q) ? 0.66
		: (id == u"spotify"_q) ? 0.39
		: (id == u"extera"_q) ? 0.00
		: (id == u"yaplus"_q) ? 0.89
		: (id == u"win95"_q) ? 0.49
		: (id == u"chibi"_q) ? 0.72
		: (id == u"chibi2"_q) ? 0.82
		: (id == u"cyan"_q) ? 0.50 : 0.04;
	for (auto y = 0; y != image.height(); ++y) {
		const auto pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
		for (auto x = 0; x != image.width(); ++x) {
			auto color = QColor::fromRgba(pixels[x]);
			if (color.hsvSaturationF() < 0.1) {
				continue;
			}
			const auto brightness = color.valueF();
			color.setHsvF(
				colorful ? std::fmod(double(x) / image.width()
					+ double(y) / image.height(), 1.) : hue,
				(monochrome || niello) ? 0. : color.hsvSaturationF(),
				(monochrome ? 0.10 + brightness * 0.05
					: niello ? 0.40 + brightness * 0.20 : brightness)
					* (dark ? 0.65 : 1.),
				color.alphaF());
			pixels[x] = color.rgba();
		}
	}
	if (id == u"classic"_q || id == u"black"_q
		|| (!id.startsWith(u"block"_q) && id != u"colorful"_q)) {
		auto clipped = QImage(image.size(), QImage::Format_ARGB32_Premultiplied);
		clipped.fill(Qt::transparent);
		auto painter = QPainter(&clipped);
		painter.setRenderHint(QPainter::Antialiasing);
		auto path = QPainterPath();
		path.addEllipse(QRectF(image.rect()));
		painter.setClipPath(path);
		painter.drawImage(QPoint(), image);
		painter.end();
		return clipped;
	}
	return image;
}

// The exported images fill the whole canvas, while macOS draws the image set
// at runtime as is and expects the margins of its icon grid.
[[nodiscard]] QImage WithMacMargins(const QImage &image) {
	if (image.isNull()) {
		return image;
	}
	auto result = QImage(
		QSize(kMacCanvasSize, kMacCanvasSize),
		QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);
	const auto skip = (kMacCanvasSize - kMacBodySize) / 2;
	auto p = QPainter(&result);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.drawImage(QRect(skip, skip, kMacBodySize, kMacBodySize), image);
	p.end();
	return result;
}

[[nodiscard]] const Loaded &Current() {
	static auto result = Loaded();
	const auto id = ForDevice().Get(kAppIcon);
	const auto alternate = AyuSettings::getInstance().appIcon();
	const auto key = id + u':' + alternate;
	if (result.id != key) {
		result = Loaded{ .id = key };
		if (id.isEmpty() && alternate != AyuAssets::DEFAULT_ICON) {
			result.light = AyuAssets::currentAppLogo();
			result.dark = result.light;
		} else {
			result.light = Load(id, false);
			result.dark = Load(id, true);
		}
		result.logo = result.light.scaled(
			kLogoSize, kLogoSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		result.logo.setDevicePixelRatio(1.);
		if (Platform::IsMac()) {
			result.light = WithMacMargins(result.light);
			result.dark = WithMacMargins(result.dark);
		}
	}
	return result;
}

[[nodiscard]] bool UseDark() {
#ifdef Q_OS_MAC
	const auto value = CFPreferencesCopyValue(
		CFSTR("AppleIconAppearanceTheme"),
		kCFPreferencesAnyApplication,
		kCFPreferencesCurrentUser,
		kCFPreferencesAnyHost);
	if (!value) {
		return false;
	}
	const auto theme = (CFGetTypeID(value) == CFStringGetTypeID())
		? QString::fromCFString(static_cast<CFStringRef>(value))
		: QString();
	CFRelease(value);
	return theme.endsWith(u"Dark"_q)
		|| (theme.endsWith(u"Automatic"_q)
			&& Core::App().settings().systemDarkMode().value_or(false));
#else // Q_OS_MAC
	return false;
#endif // Q_OS_MAC
}

void Refresh() {
	++Generation;
	Core::App().refreshApplicationIcon();
	Core::App().enumerateWindows([](not_null<Window::Controller*> window) {
		window->widget()->updateWindowIcon();
	});
	Core::App().tray().updateIconCounters();
}

} // namespace

std::vector<AppIconChoice> AppIconChoices() {
	auto result = std::vector<AppIconChoice>();
	result.push_back({ QString(), Title(QStringView()) });
	for (const auto id : kAppIconIds) {
		result.push_back({ QString::fromUtf16(id), Title(id) });
	}
	return result;
}

QString AppIconTitle(const QString &id) {
	return Title(id);
}

QImage AppIconPreview(const QString &id) {
	return Load(id, false);
}

const QImage *CustomLogo() {
	const auto &current = Current();
	return current.logo.isNull() ? nullptr : &current.logo;
}

QIcon CustomAppIcon() {
	if (Platform::IsMac() && ForDevice().Get(kAppIcon).isEmpty()
		&& AyuSettings::getInstance().appIcon() == AyuAssets::DEFAULT_ICON) {
		return QIcon();
	}
	const auto &current = Current();
	if (current.light.isNull()) {
		return QIcon();
	}
	const auto dark = !current.dark.isNull() && UseDark();
	return QIcon(Ui::PixmapFromImage(
		base::duplicate(dark ? current.dark : current.light)));
}

int AppIconGeneration() {
	return Generation;
}

QRectF TrayUnreadDot(QSize size) {
	// The dot of tray_monochrome_attention.svg, on its 16 unit grid.
	const auto x = size.width() / 16.;
	const auto y = size.height() / 16.;
	return QRectF(1.7 * x, 10.5 * y, 4.4 * x, 4.4 * y);
}

void StartAppIcon(rpl::lifetime &lifetime) {
	if (const auto icon = CustomAppIcon(); !icon.isNull()) {
		Platform::SetApplicationIcon(icon);
	}
	ForDevice().changes(
	) | rpl::filter([](auto key) {
		return key == kAppIcon.key;
	}) | rpl::on_next([] {
		Refresh();
	}, lifetime);
	if (Platform::IsMac()) {
		Core::App().settings().systemDarkModeChanges(
		) | rpl::on_next([] {
			if (CustomLogo()) {
				Core::App().refreshApplicationIcon();
			}
		}, lifetime);
	}
}

} // namespace Nagram::Interface
