// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/ui/ayu_logo.h"

#include "regram/ayu/ayu_settings.h"
#include "regram/nagram/interface/app_icon.h"
#include "styles/style_ayu_styles.h"
#include "ui/rect.h"

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QSaveFile>

static QString LAST_LOADED_NAME;
static QImage LAST_LOADED;
static QImage LAST_LOADED_PAD;

namespace AyuAssets {

QString appIcoPath() {
	const auto &settings = AyuSettings::getInstance();
	return cWorkingDir()
		+ u"tdata/Regram-"_q
		+ settings.appIcon()
		+ u".ico"_q;
}

QByteArray EncodeIco(const QImage &image) {
	constexpr auto kIconSize = 256;
	constexpr auto kPayloadOffset = 22;
	auto png = QByteArray();
	auto output = QBuffer(&png);
	if (!output.open(QIODevice::WriteOnly)
		|| !image.scaled(kIconSize, kIconSize, Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation).save(&output, "PNG")) {
		return {};
	}
	auto result = QByteArray();
	auto stream = QDataStream(&result, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);
	stream << quint16(0) << quint16(1) << quint16(1)
		<< quint8(0) << quint8(0) << quint8(0) << quint8(0)
		<< quint16(1) << quint16(32)
		<< quint32(png.size()) << quint32(kPayloadOffset);
	if (stream.writeRawData(png.constData(), png.size()) != png.size()
		|| stream.status() != QDataStream::Ok) {
		return {};
	}
	return result;
}

void loadAppIco() {
	const auto bytes = EncodeIco(currentAppLogo());
	auto file = QSaveFile(appIcoPath());
	if (bytes.isEmpty() || !file.open(QIODevice::WriteOnly)
		|| file.write(bytes) != bytes.size()
		|| !file.commit()) {
		LOG(("Regram: could not save application icon."));
	}
}

QImage CreateImage(
		const QString &name,
		QSize resultImageSize,
		int padding = 0) {
	const auto iconSize = resultImageSize.shrunkBy(
		QMargins(padding, padding, padding, padding));
	const auto loaded = Nagram::Interface::AppIconPreview(
		name == DEFAULT_ICON ? QString() : name);
	if (loaded.isNull()) {
		return {};
	}
	auto result = QImage(
		resultImageSize * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(style::DevicePixelRatio());
	result.fill(Qt::transparent);
	{
		auto painter = QPainter(&result);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawImage(
			QRect(QPoint(padding, padding), iconSize), loaded);
	}
	return result;
}

void loadIcons() {
	const auto &settings = AyuSettings::getInstance();
	if (LAST_LOADED_NAME != settings.appIcon()) {
		LAST_LOADED_NAME = settings.appIcon();
		LAST_LOADED = CreateImage(settings.appIcon(), Size(256));
		LAST_LOADED_PAD = CreateImage(settings.appIcon(), Size(256), 12);
	}
}

QImage loadPreview(const QString &name) {
	return CreateImage(name, Size(st::iconPickerIconSize), st::iconPickerImagePadding);
}

QString currentAppLogoName() {
	return LAST_LOADED_NAME;
}

QImage currentAppLogo() {
	loadIcons();
	return LAST_LOADED;
}

QImage currentAppLogoPad() {
	loadIcons();
	return LAST_LOADED_PAD;
}

}
