#include "regram/typography/font_data.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <cstdlib>
#include <iostream>
#include <map>

namespace {

void Check(bool passed, const char *name) {
	if (!passed) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit(1);
	}
}

QByteArray Read(const QString &path) {
	auto file = QFile(path);
	Check(file.open(QIODevice::ReadOnly), "font fixture opens");
	return file.readAll();
}

} // namespace

int main(int argc, char *argv[]) {
	auto app = QGuiApplication(argc, argv);
	const auto catalog = QJsonDocument::fromJson(Read(QString::fromUtf8(REGRAM_FONT_CATALOG))).object();
	const auto families = catalog.value(QStringLiteral("families")).toArray();
	const auto files = catalog.value(QStringLiteral("files")).toArray();
	Check(families.size() == 17 && files.size() == 92, "complete iOS font catalog retained");
	if (argc == 2) {
		const auto root = QString::fromLocal8Bit(argv[1]) + '/';
		auto loaded = 0;
		auto names = std::map<QString, QString>();
		for (const auto entry : files) {
			const auto file = entry.toObject();
			const auto bytes = Read(root + file.value(QStringLiteral("file")).toString());
			Check(bytes.size() == file.value(QStringLiteral("bytes")).toInteger(),
				"downloaded font size matches iOS catalog");
			Check(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
				== file.value(QStringLiteral("sha256")).toString().toLatin1(),
				"downloaded font hash matches iOS catalog");
			const auto id = QFontDatabase::addApplicationFontFromData(Regram::Typography::NativeFontData(bytes));
			if (id < 0) {
				std::cerr << "Unsupported font: " << file.value(QStringLiteral("file")).toString().toStdString() << '\n';
			}
			Check(id >= 0 && !QFontDatabase::applicationFontFamilies(id).empty(),
				"native Qt loads actual downloaded font faces");
			names.emplace(file.value(QStringLiteral("file")).toString(),
				QFontDatabase::applicationFontFamilies(id).front());
			++loaded;
		}
		auto preview = QImage(960, 700, QImage::Format_ARGB32_Premultiplied);
		preview.fill(Qt::white);
		auto painter = QPainter(&preview);
		auto y = 0;
		for (const auto &name : { QStringLiteral("JetBrainsMono-Regular.ttf"),
			QStringLiteral("GoogleSans-Variable.ttf"), QStringLiteral("AnthropicSans-Variable.woff2"),
			QStringLiteral("AnthropicSerif-Variable.woff2"), QStringLiteral("IBMPlexSansSC-Regular.ttf"),
			QStringLiteral("NotoSerifSC-Variable.ttf") }) {
			const auto family = names.at(name);
			auto font = QFont(family);
			font.setPixelSize(30);
			painter.setFont(font);
			painter.drawText(QRect(20, y, 920, 110), Qt::TextWordWrap,
				family + QStringLiteral("\nRegram Aa Bb 123 · 中文字体预览"));
			y += 110;
		}
		painter.end();
		Check(preview.save(QStringLiteral("font-preview.png")), "font preview renders");
		std::cout << "Native font validation: " << loaded << " files passed\n";
	}
	std::cout << "Regram typography tests passed\n";
}
