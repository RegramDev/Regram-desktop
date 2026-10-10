#include <QtGui/QImage>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QTextLayout>
#include <QtCore/QCryptographicHash>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <iostream>
#include <memory>

int main(int argc, char *argv[]) {
	auto application = (argc > 1 && QByteArray(argv[1]) == "--text")
		? std::make_unique<QGuiApplication>(argc, argv)
		: std::unique_ptr<QGuiApplication>();
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
	auto report = QJsonObject{
		{ "pixel_cases", results },
		{ "blend_frames", 30 },
		{ "blend_ms", timer.nsecsElapsed() / 1000000.0 },
	};
	if (application) {
		auto textCases = QJsonArray();
		auto font = QFont(QString::fromUtf16(u"Arial"));
		font.setPixelSize(16);
		for (const auto text : {
			u"Latin letters with ligatures: office, affinity, 1234567890.",
			u"中文长消息：滚动时保持文字的布局、换行和字形一致。",
			u"العربية mixed English עברית 12345 — مرحبا بالعالم",
			u"देवनागरी पाठ: नमस्ते दुनिया, क्षत्रिय, कर्म।",
			u"Emoji 👩‍💻 👨‍👩‍👧‍👦 🏳️‍🌈 ❤️ combined e\u0301 a\u0308.",
		}) {
			for (const auto ratio : { 1, 2 }) {
				auto image = QImage(QSize(320, 160) * ratio,
					QImage::Format_ARGB32_Premultiplied);
				image.setDevicePixelRatio(ratio);
				auto geometry = QJsonArray();
				const auto draw = [&] {
					image.fill(Qt::white);
					auto layout = QTextLayout(QString::fromUtf16(text), font);
					layout.beginLayout();
					auto top = qreal(0);
					geometry = QJsonArray();
					while (true) {
						auto line = layout.createLine();
						if (!line.isValid()) {
							break;
						}
						line.setLineWidth(300);
						line.setPosition(QPointF(0, top));
						geometry.push_back(QJsonArray{
							line.textStart(), line.textLength(),
							line.naturalTextWidth(), line.height(),
							line.cursorToX(line.textStart()),
						});
						top += line.height();
					}
					layout.endLayout();
					auto painter = QPainter(&image);
					painter.setPen(Qt::black);
					layout.draw(&painter, QPointF(10, 10));
				};
				draw();
				timer.restart();
				for (auto frame = 0; frame != 100; ++frame) {
					draw();
				}
				const auto elapsed = timer.nsecsElapsed() / 1000000.0;
				const auto hash = QCryptographicHash::hash(
					QByteArray(reinterpret_cast<const char*>(image.constBits()),
						image.sizeInBytes()),
					QCryptographicHash::Sha256).toHex();
				textCases.push_back(QJsonObject{
					{ "text", QString::fromUtf16(text) },
					{ "dpr", ratio },
					{ "sha256", QString::fromLatin1(hash) },
					{ "geometry", geometry },
					{ "frames", 100 },
					{ "ms", elapsed },
				});
			}
		}
		report.insert("text_cases", textCases);
	}
	std::cout << QJsonDocument(report).toJson(QJsonDocument::Compact).constData()
		<< std::endl;
}
