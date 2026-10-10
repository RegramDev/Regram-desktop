#include "regram/nagram/filters/model.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>

#include <iostream>

namespace {

class MemoryPrefs final : public Nagram::RawPrefs {
public:
	QByteArray read(std::string_view key) override {
		return raw;
	}
	void write(std::string_view key, const QByteArray &value) override {
		raw = value;
	}
	void clear(std::string_view key) override {
		raw.clear();
	}

	QByteArray raw;

};

} // namespace

void BenchmarkFilters() {
	using namespace Nagram;
	auto config = Filters::Defaults();
	config.insert(u"enabled"_q, true);
	auto rules = QJsonArray();
	for (auto i = 0; i != Filters::kMaxRules; ++i) {
		rules.push_back(QJsonObject{
			{ u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces) },
			{ u"title"_q, QString::number(i) },
			{ u"pattern"_q, u"unmatched-pattern-"_q + QString::number(i) },
			{ u"replacement"_q, QString() },
			{ u"enabled"_q, true },
			{ u"caseInsensitive"_q, false },
			{ u"reversed"_q, false },
			{ u"action"_q, u"hide"_q },
		});
	}
	config.insert(u"rules"_q, rules);
	auto prefs = MemoryPrefs();
	prefs.raw = QJsonDocument(config).toJson(QJsonDocument::Compact);
	auto options = Options(prefs, Scope::Account);
	auto timer = QElapsedTimer();
	timer.start();
	auto bytes = qsizetype(0);
	for (auto i = 0; i != 10'000; ++i) {
		bytes += options.Get(Filters::kRules).size();
	}
	const auto readNs = timer.nsecsElapsed();
	timer.restart();
	auto hidden = 0;
	for (auto i = 0; i != 1'000; ++i) {
		hidden += Filters::Apply(prefs.raw,
			{ u"Fixture message "_q + QString::number(i) },
			{}, {}, false, false).hidden;
	}
	const auto result = QJsonDocument(QJsonObject{
		{ u"option_reads"_q, 10'000 },
		{ u"option_read_ms"_q, double(readNs) / 1'000'000 },
		{ u"messages"_q, 1'000 },
		{ u"rules"_q, Filters::kMaxRules },
		{ u"apply_ms"_q, double(timer.nsecsElapsed()) / 1'000'000 },
		{ u"bytes"_q, double(bytes) },
		{ u"hidden"_q, hidden },
	});
	std::cout << result.toJson(QJsonDocument::Compact).constData() << std::endl;
}
