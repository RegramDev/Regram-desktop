#include "regram/nagram/links/model.h"
#include "regram/nagram/links/webview.h"
#include "base/basic_types.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <map>
#include <string>
#include <stdexcept>

namespace {

class MemoryPrefs final : public Nagram::RawPrefs {
public:
	[[nodiscard]] QByteArray read(std::string_view key) override {
		const auto found = values.find(std::string(key));
		return (found == values.end()) ? QByteArray() : found->second;
	}
	void write(std::string_view key, const QByteArray &value) override {
		values[std::string(key)] = value;
	}
	void clear(std::string_view key) override {
		values.erase(std::string(key));
	}

	std::map<std::string, QByteArray> values;
};

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

void TestWebviewPattern() {
	using namespace Nagram;
	using namespace Nagram::Links;
	const auto start = u"https://app.example/start?x=1#tgWebAppData=secret"_q;
	const auto run = [&](const QString &pattern, const QString &uri) {
		return MatchOutsideWebview(
			CompileWebviewPattern(pattern), uri, start).open;
	};
	const auto pattern = u"^https://(docs|pay)\\.example/"_q;
	Require(run(pattern, u"https://docs.example/page"_q)
		&& run(pattern, u"HTTPS://DOCS.EXAMPLE/page"_q)
		&& run(u"example"_q, u"http://other.example/"_q),
		"matching http and https addresses must open outside");
	Require(!run(pattern, u"https://app.example/inside"_q),
		"address that does not match was opened outside");
	const auto any = u".*"_q;
	const auto kept = std::vector<std::pair<QString, const char*>>{
		{ u"tg://resolve?domain=example"_q, "tg link opened outside" },
		{ u"javascript:alert(1)"_q, "javascript link opened outside" },
		{ u"file:///etc/hosts"_q, "file link opened outside" },
		{ u"about:blank"_q, "about link opened outside" },
		{ u"data:text/html,x"_q, "data link opened outside" },
		{ start, "start address opened outside" },
		{ u"https://app.example/start?x=1"_q,
			"start address without its fragment opened outside" },
		{ u"https://app.example/start?x=1#other"_q,
			"start address with another fragment opened outside" },
		{ u"https://docs.example/?tgWebAppData=secret"_q,
			"launch data in the query opened outside" },
		{ u"https://docs.example/#tgWebAppData=secret&x=1"_q,
			"launch data in the fragment opened outside" },
		{ u"https://docs.example/"_q + QString(9000, u'a'),
			"oversized address opened outside" },
		{ QString(), "empty address opened outside" },
	};
	for (const auto &[uri, message] : kept) {
		Require(!run(any, uri), message);
	}

	auto clock = QElapsedTimer();
	clock.start();
	const auto slow = MatchOutsideWebview(
		CompileWebviewPattern(u"(a+)+$"_q),
		u"https://docs.example/"_q + QString(40, u'a') + u'!',
		start);
	Require(!slow.open && !slow.error.isEmpty() && clock.elapsed() < 500,
		"a runaway pattern must stay inside and report its limit");

	Require(ValidWebviewPattern(QString())
		&& ValidWebviewPattern(pattern)
		&& !ValidWebviewPattern(u"("_q)
		&& !ValidWebviewPattern(u"[a-z&&[^x]]"_q)
		&& !ValidWebviewPattern(QString(kMaxWebviewPattern + 1, u'a')),
		"web app link pattern validation");
	const auto problem = CheckWebviewPattern(u"abc("_q);
	Require(problem && problem->position >= 3 && !problem->text.isEmpty(),
		"invalid pattern must report its position");

	auto last = qint64();
	Require(AllowOutsideOpen(last, 5000)
		&& !AllowOutsideOpen(last, 5999)
		&& last == 5000
		&& AllowOutsideOpen(last, 6000)
		&& !AllowOutsideOpen(last, 6001),
		"outside opens must be limited to one per second");

	auto registry = Registry();
	RegisterWebviewOptions(registry);
	Require(registry.HasFlag(kWebviewExternalPattern.key, Flag::Exportable)
		&& kWebviewExternalPattern.fallback.isEmpty(),
		"web app link pattern must be an exportable option, off by default");
	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(options.Set(kWebviewExternalPattern, pattern)
		&& options.Get(kWebviewExternalPattern) == pattern
		&& !options.Set(kWebviewExternalPattern, u"("_q)
		&& options.Get(kWebviewExternalPattern) == pattern
		&& options.Set(kWebviewExternalPattern, QString())
		&& prefs.values.empty(),
		"web app link pattern writes");
	prefs.values[std::string(kWebviewExternalPattern.key)] = "s(";
	Require(options.Get(kWebviewExternalPattern).isEmpty()
		&& options.invalidKeys().contains(kWebviewExternalPattern.key)
		&& prefs.values[std::string(kWebviewExternalPattern.key)] == "s(",
		"invalid stored pattern must fall back and stay stored");
}

} // namespace

void TestLinks() {
	using namespace Nagram::Links;
	const auto newRule = NewRule(u"example.com"_q,
		u"mirror.example"_q, { u"utm_*"_q });
	Require(!newRule.value(u"enabled"_q).toBool(),
		"new link rule is enabled by default");
	auto config = Defaults();
	config.insert(u"rules"_q, QJsonArray{ newRule });
	auto raw = QJsonDocument(config).toJson(QJsonDocument::Compact);
	Require(Validate(raw), "valid link rule rejected");
	const auto original = u"http://example.com/p?utm_source=x&keep=y#f"_q;
	Require(!Rewrite(raw, original).changed,
		"disabled new rule changed link");
	auto enabled = newRule;
	enabled.insert(u"enabled"_q, true);
	config.insert(u"rules"_q, QJsonArray{ enabled });
	raw = QJsonDocument(config).toJson(QJsonDocument::Compact);
	const auto changed = Rewrite(raw, original);
	Require(changed.error.isEmpty() && changed.changed
		&& changed.url.toString(QUrl::FullyEncoded)
			== u"https://mirror.example/p?keep=y#f"_q,
		"link rewrite changed the wrong URL components");
	Require(!Rewrite(raw, u"https://other.example/p"_q).changed,
		"rule matched a different host");
	Require(!Rewrite(raw, u"http://name:password@example.com/p"_q).error.isEmpty(),
		"credential-bearing URL was rewritten");
	TestWebviewPattern();
	std::cout << "PASS: Nagram link rules and web app link pattern"
		<< std::endl;
}
