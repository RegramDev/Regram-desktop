#include "regram/nagram/links/inline_rules.h"

#include "base/basic_types.h"
#include "regram/nagram/core/exchange.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

using namespace Nagram;
using namespace Nagram::Links;

class MemoryPrefs final : public RawPrefs {
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

QJsonObject Rule(
		const QString &username,
		const QStringList &patterns,
		bool enabled = true) {
	auto rule = NewInlineRule(username, patterns);
	rule.insert(u"enabled"_q, enabled);
	return rule;
}

QByteArray Config(const QJsonArray &rules) {
	auto config = InlineDefaults();
	config.insert(u"rules"_q, rules);
	return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

QString Auto(const QByteArray &raw, const QString &text, bool formatted) {
	return AutomaticInlineBot(
		true,
		CompileInlineRules(raw),
		text,
		formatted).username;
}

void TestTrigger() {
	const auto raw = Config(QJsonArray{
		Rule(u"video_bot"_q, { u"^https?://video\\.example/\\S+"_q }),
	});
	const auto link = u"https://video.example/watch?v=1"_q;
	Require(Auto(raw, link, false) == u"video_bot"_q
		&& Auto(raw, u"  "_q + link + u"\n"_q, false) == u"video_bot"_q,
		"a single matching link must select the rule's bot");
	const auto rejected = std::vector<std::pair<QString, const char*>>{
		{ link + u" hello"_q, "link followed by text matched" },
		{ u"look "_q + link, "link after text matched" },
		{ link + u" "_q + link, "two links matched" },
		{ link + u"\n"_q + link, "two lines of links matched" },
		{ u"@video_bot "_q + link, "explicit inline query matched" },
		{ u"ftp://video.example/watch"_q, "non-http scheme matched" },
		{ u"tg://resolve?domain=video"_q, "tg scheme matched" },
		{ u"HTTPS://video.example/watch"_q, "upper-case scheme matched" },
		{ u"video.example/watch"_q, "link without a scheme matched" },
		{ u"https://other.example/watch"_q, "unrelated link matched" },
		{ u"https://video.example/"_q + QString(kMaxInlineLink, u'a'),
			"link above 2048 characters matched" },
		{ QString(), "empty text matched" },
	};
	for (const auto &[text, message] : rejected) {
		Require(Auto(raw, text, false).isEmpty(), message);
	}
	Require(Auto(raw, link, true).isEmpty(),
		"formatted text must not start an automatic query");
	Require(AutomaticInlineBot(
		false, CompileInlineRules(raw), link, false).username.isEmpty(),
		"the master switch must turn automatic queries off");
	Require(Auto({}, link, false).isEmpty()
		&& Auto("broken", link, false).isEmpty(),
		"empty or invalid rules must not match");
}

void TestOrder() {
	const auto link = u"https://example.com/a"_q;
	const auto any = QStringList{ u"example\\.com"_q };
	Require(Auto(Config(QJsonArray{
		Rule(u"first_bot"_q, any),
		Rule(u"second_bot"_q, any),
	}), link, false) == u"first_bot"_q,
		"the first matching rule must win");
	Require(Auto(Config(QJsonArray{
		Rule(u"first_bot"_q, any, false),
		Rule(u"second_bot"_q, any),
	}), link, false) == u"second_bot"_q,
		"a disabled rule must not take part");
	Require(Auto(Config(QJsonArray{
		Rule(u"first_bot"_q, { u"nothing"_q, u"example\\.com/a$"_q }),
	}), link, false) == u"first_bot"_q,
		"any pattern of a rule may match");
	Require(!NewInlineRule(u"some_bot"_q, any).value(u"enabled"_q).toBool(),
		"a new inline bot rule must start disabled");

	const auto lookahead = QStringList{
		u"^https?://github\\.com/(?!\\S+\\.git)[\\w.-]+/[\\w.-]+/?$"_q,
	};
	Require(!CheckInlinePatterns(lookahead)
		&& Auto(Config(QJsonArray{ Rule(u"repo_bot"_q, lookahead) }),
			u"https://github.com/owner/project"_q, false) == u"repo_bot"_q
		&& Auto(Config(QJsonArray{ Rule(u"repo_bot"_q, lookahead) }),
			u"https://github.com/owner/project.git"_q, false).isEmpty(),
		"a lookahead rule must compile and match its sample link");
}

void TestValidation() {
	const auto pattern = QStringList{ u"example\\.com"_q };
	Require(ValidateInlineRules({})
		&& ValidateInlineRules(Config({}))
		&& ValidateInlineRules(Config(QJsonArray{
			Rule(u"Some_Bot1"_q, pattern),
			Rule(u"other_bot"_q, { u"a"_q, u"b"_q }, false),
		})),
		"valid inline bot rules rejected");

	const auto with = [&](const QString &key, const QJsonValue &value) {
		auto rule = Rule(u"some_bot"_q, pattern);
		rule.insert(key, value);
		return Config(QJsonArray{ rule });
	};
	auto unknownTop = InlineDefaults();
	unknownTop.insert(u"extra"_q, 1);
	auto wrongVersion = InlineDefaults();
	wrongVersion.insert(u"version"_q, 2);
	auto many = QJsonArray();
	for (auto i = 0; i != 65; ++i) {
		many.push_back(Rule(u"bot_%1"_q.arg(i), pattern));
	}
	auto manyPatterns = QJsonArray();
	for (auto i = 0; i != 17; ++i) {
		manyPatterns.push_back(u"a"_q);
	}
	const auto same = Rule(u"some_bot"_q, pattern);
	auto sameId = Rule(u"other_bot"_q, pattern);
	sameId.insert(u"id"_q, same.value(u"id"_q));
	const auto bad = std::vector<std::pair<QByteArray, const char*>>{
		{ "[]", "non-object inline rules accepted" },
		{ "{\"version\":1", "truncated inline rules accepted" },
		{ QJsonDocument(wrongVersion).toJson(), "version 2 accepted" },
		{ QJsonDocument(unknownTop).toJson(), "unknown field accepted" },
		{ with(u"extra"_q, 1), "unknown rule field accepted" },
		{ with(u"id"_q, u"1"_q), "malformed rule id accepted" },
		{ with(u"enabled"_q, 1), "numeric enabled value accepted" },
		{ with(u"username"_q, u"bot"_q), "3-character username accepted" },
		{ with(u"username"_q, u"1some_bot"_q), "leading digit accepted" },
		{ with(u"username"_q, u"some-bot"_q), "dash in username accepted" },
		{ with(u"username"_q, u"@some_bot"_q), "at sign accepted" },
		{ with(u"username"_q, QString(33, u'a')), "33 characters accepted" },
		{ with(u"username"_q, 5), "numeric username accepted" },
		{ with(u"patterns"_q, QJsonArray()), "empty pattern list accepted" },
		{ with(u"patterns"_q, manyPatterns), "17 patterns accepted" },
		{ with(u"patterns"_q, QJsonArray{ QString() }),
			"empty pattern accepted" },
		{ with(u"patterns"_q, QJsonArray{ QString(513, u'a') }),
			"513-character pattern accepted" },
		{ with(u"patterns"_q, QJsonArray{ u"a\nb"_q }),
			"pattern with a line break accepted" },
		{ with(u"patterns"_q, QJsonArray{ 1 }), "numeric pattern accepted" },
		{ with(u"patterns"_q, u"a"_q), "string pattern list accepted" },
		{ with(u"patterns"_q, QJsonArray{ u"("_q }),
			"pattern that does not compile accepted" },
		{ with(u"patterns"_q, QJsonArray{ u"[a-z&&[^x]]"_q }),
			"Java class intersection accepted" },
		{ with(u"patterns"_q, QJsonArray{ u"\\p{javaLowerCase}"_q }),
			"Java character property accepted" },
		{ Config(many), "65 rules accepted" },
		{ Config(QJsonArray{ same, sameId }), "duplicate id accepted" },
		{ Config(QJsonArray{
			Rule(u"some_bot"_q, pattern),
			Rule(u"SOME_BOT"_q, pattern),
		}), "duplicate username accepted" },
	};
	for (const auto &[raw, message] : bad) {
		Require(!ValidateInlineRules(raw), message);
	}

	const auto second = CheckInlinePatterns({ u"ok"_q, u"(broken"_q });
	Require(second && second->line == 2 && !second->text.isEmpty(),
		"a pattern error must name the failing line");
	Require(CheckInlinePatterns({}).has_value(),
		"an empty pattern list must be reported");
}

void TestBudget() {
	auto clock = QElapsedTimer();
	clock.start();
	const auto runaway = AutomaticInlineBot(
		true,
		CompileInlineRules(Config(QJsonArray{
			Rule(u"slow_bot"_q, { u"(a+)+$"_q }),
			Rule(u"next_bot"_q, { u"example"_q }),
		})),
		u"https://example.com/"_q + QString(40, u'a') + u'!',
		false);
	Require(runaway.username.isEmpty() && !runaway.error.isEmpty(),
		"a runaway pattern must report its limit and match nothing");

	auto full = QJsonArray();
	auto patterns = QStringList();
	for (auto i = 0; i != 16; ++i) {
		patterns.push_back(u"^https?://host%1\\.example/[a-z]+/\\d+$"_q.arg(i));
	}
	for (auto i = 0; i != 64; ++i) {
		full.push_back(Rule(u"bot_%1"_q.arg(i), patterns));
	}
	const auto raw = Config(full);
	Require(ValidateInlineRules(raw), "full inline rule set rejected");
	const auto matcher = CompileInlineRules(raw);
	const auto link = (u"https://unrelated.example/"_q
		+ QString(kMaxInlineLink, u'a')).left(kMaxInlineLink);
	clock.restart();
	const auto result = AutomaticInlineBot(true, matcher, link, false);
	Require(clock.elapsed() < 200,
		"a full rule set exceeded the matching time limit");
	Require(result.username.isEmpty(), "a full rule set matched nothing");
}

void TestImport() {
	auto registry = Registry();
	RegisterInlineOptions(registry);
	Require(registry.HasFlag(kAutoInlineBot.key, Flag::Exportable)
		&& registry.HasFlag(kInlineBotRules.key, Flag::Exportable)
		&& registry.Find(kInlineBotRules.key)->scope == Scope::Device,
		"inline bot options must be exportable device options");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Get(kAutoInlineBot)
		&& options.Get(kInlineBotRules).isEmpty(),
		"inline bot options must default to off and empty");
	const auto raw = Config(QJsonArray{
		Rule(u"some_bot"_q, { u"example\\.com"_q }),
	});
	Require(options.Set(kInlineBotRules, raw)
		&& options.Set(kAutoInlineBot, true)
		&& !options.Set(kInlineBotRules, QByteArray("{}")),
		"inline bot option writes");
	prefs.values[std::string(kInlineBotRules.key)] = "broken";
	Require(options.Get(kInlineBotRules).isEmpty()
		&& options.invalidKeys().contains(kInlineBotRules.key)
		&& prefs.values[std::string(kInlineBotRules.key)] == "broken",
		"invalid inline bot rules must fall back and stay stored");
	prefs.values[std::string(kInlineBotRules.key)] = raw;

	const auto disabled = DisableInlineRules(raw);
	Require(ValidateInlineRules(disabled)
		&& DisableInlineRules(disabled) == disabled
		&& CompileInlineRules(disabled).entries.empty()
		&& DisableInlineRules("broken") == "broken",
		"disabling imported rules must be stable");

	const auto exported = Exchange::Export(options, registry);
	Require(exported.invalidKeys.isEmpty(), "inline bot rules export");
	auto importedPrefs = MemoryPrefs();
	auto imported = Options(importedPrefs);
	const auto plan = Exchange::PlanImport(imported, registry, exported.data);
	Require(plan.error.isEmpty() && plan.changes.size() == 2
		&& plan.adjustedKeys == QStringList{ u"nagram.inlineBotRules"_q },
		"import preview must report the adjusted inline bot rules");
	Require(Exchange::Apply(imported, registry, plan).applied
		&& imported.Get(kAutoInlineBot)
		&& imported.Get(kInlineBotRules) == disabled,
		"imported inline bot rules must be turned off");
	Require(Exchange::PlanImport(imported, registry, exported.data)
		.changes.empty(),
		"importing the same file twice must change nothing");
}

} // namespace

void TestInlineRules() {
	TestTrigger();
	TestOrder();
	TestValidation();
	TestBudget();
	TestImport();
	std::cout << "PASS: Nagram inline bot rules" << std::endl;
}
