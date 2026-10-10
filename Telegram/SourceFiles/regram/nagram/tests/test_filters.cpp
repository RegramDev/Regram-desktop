#include "regram/nagram/filters/model.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

QJsonObject Rule(const QString &pattern, const QString &action) {
	return {
		{ u"id"_q, u"00000000-0000-0000-0000-000000000001"_q },
		{ u"title"_q, u"test"_q },
		{ u"pattern"_q, pattern },
		{ u"replacement"_q, u"bar"_q },
		{ u"action"_q, action },
		{ u"enabled"_q, true },
		{ u"caseInsensitive"_q, false },
		{ u"reversed"_q, false },
	};
}

QByteArray Config(const QJsonObject &rule) {
	auto config = Nagram::Filters::Defaults();
	config.insert(u"enabled"_q, true);
	config.insert(u"rules"_q, QJsonArray{ rule });
	return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

} // namespace

void TestFilters() {
	using namespace Nagram::Filters;
	Require(Validate({}), "empty filter config rejected");
	const auto replace = Config(Rule(u"foo"_q, u"replace"_q));
	Require(Validate(replace), "valid filter config rejected");
	const auto replaced = Apply(replace, { u"foo foo"_q }, {}, {}, false, false);
	Require(replaced.text.text == u"bar bar"_q && replaced.matches == 2,
		"replacement or match count incorrect");
	const auto hidden = Apply(Config(Rule(u"foo"_q, u"hide"_q)),
		{ u"foo"_q }, {}, {}, false, false);
	Require(hidden.hidden, "hide action did not hide locally");
	Require(!Validate(Config(Rule(u"("_q, u"hide"_q))),
		"invalid regex accepted");
	const auto excessive = Apply(Config(Rule(u"a"_q, u"replace"_q)),
		{ QString(300, u'a') }, {}, {}, false, false);
	Require(!excessive.error.isEmpty() && excessive.text.text == QString(300, u'a'),
		"match budget did not preserve original text");
	auto clock = QElapsedTimer();
	clock.start();
	const auto adversarial = Apply(
		Config(Rule(u"(a+)+$"_q, u"hide"_q)),
		{ QString(16000, u'a') + u"X"_q }, {}, {}, false, false);
	Require(clock.elapsed() < 500,
		"adversarial regex exceeded bounded test duration");
	Require(!adversarial.hidden && !adversarial.error.isEmpty(),
		"regex work limit did not report an actionable failure");
	const auto previous = Config(Rule(u"cached-original"_q, u"hide"_q));
	Require(Apply(previous, { u"cached-original"_q }, {}, {}, false, false).hidden,
		"initial cached configuration failed");
	for (auto i = 0; i != 64; ++i) {
		const auto pattern = u"cached-"_q + QString::number(i);
		const auto changed = Config(Rule(pattern, u"hide"_q));
		Require(Apply(changed, { pattern }, {}, {}, false, false).hidden
			&& !Apply(changed, { u"cached-original"_q }, {}, {}, false, false).hidden,
			"compiled filters reused another configuration");
	}
	Require(Apply(previous, { u"cached-original"_q }, {}, {}, false, false).hidden,
		"evicted filter did not compile again");
	Require(!Apply("invalid", { u"cached-original"_q }, {}, {}, false, false)
		.error.isEmpty(), "invalid filter reused a valid cached configuration");
	std::cout << "PASS: Nagram filter budgets and projection" << std::endl;
}
