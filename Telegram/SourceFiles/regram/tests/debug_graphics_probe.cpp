#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtCore/QCryptographicHash>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <iostream>

int main() {
	auto results = QJsonArray();
	for (const auto format : {
		QImage::Format_ARGB32_Premultiplied,
		QImage::Format_RGB32,
		QImage::Format_RGBA8888_Premultiplied,
		QImage::Format_RGBA64_Premultiplied,
	}) {
		auto destination = QImage(511, 383, format);
		destination.fill(QColor(57, 98, 144, 201));
		auto source = QImage(71, 61, QImage::Format_ARGB32_Premultiplied);
		source.fill(QColor(219, 56, 133, 163));
		{
			auto painter = QPainter(&source);
			auto gradient = QLinearGradient(0, 0, 71, 61);
			gradient.setColorAt(0, QColor(61, 244, 170, 89));
			gradient.setColorAt(1, QColor(5, 2, 233, 177));
			painter.fillRect(source.rect(), gradient);
		}
		for (auto mode = 0; mode <= int(QPainter::CompositionMode_Exclusion); ++mode) {
			auto painter = QPainter(&destination);
			painter.setCompositionMode(QPainter::CompositionMode(mode));
			painter.setOpacity(.73);
			painter.fillRect(QRect(7 + mode, 5 + mode, 303, 207),
				QColor(35, 137, 253, 127));
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
			painter.drawImage(
				QRectF(43.5, 55.25, 238.5, 142.75),
				source,
				QRectF(0, 0, source.width(), source.height()));
		}
		const auto hash = QCryptographicHash::hash(
			QByteArray(
				reinterpret_cast<const char*>(destination.constBits()),
				destination.sizeInBytes()),
			QCryptographicHash::Sha256).toHex();
		results.push_back(QJsonObject{
			{ "format", int(format) },
			{ "sha256", QString::fromLatin1(hash) },
		});
	}
	auto frame = QImage(2400, 1700, QImage::Format_ARGB32_Premultiplied);
	frame.fill(QColor(97, 174, 225));
	const auto image = frame.copy();
	auto timer = QElapsedTimer();
	timer.start();
	for (auto i = 0; i != 30; ++i) {
		auto painter = QPainter(&frame);
		painter.fillRect(frame.rect(), QColor(0, 0, 0, 92));
		painter.setOpacity(.61);
		painter.drawImage(0, 0, image);
	}
	std::cout << QJsonDocument(QJsonObject{
		{ "pixel_cases", results },
		{ "blend_frames", 30 },
		{ "blend_ms", timer.nsecsElapsed() / 1000000.0 },
	}).toJson(QJsonDocument::Compact).constData() << std::endl;
}
