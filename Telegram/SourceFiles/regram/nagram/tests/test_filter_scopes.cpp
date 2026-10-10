#include "regram/nagram/filters/model.h"

#include "regram/nagram/core/regex.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

using namespace Nagram;
using namespace Nagram::Filters;

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

QString Id(int index) {
	return u"00000000-0000-0000-0000-%1"_q.arg(index, 12, 10, QChar(u'0'));
}

QJsonObject Rule(
		int index,
		const QString &pattern,
		const QString &replacement,
		bool enabled = true) {
	return {
		{ u"id"_q, Id(index) },
		{ u"title"_q, u"rule %1"_q.arg(index) },
		{ u"pattern"_q, pattern },
		{ u"replacement"_q, replacement },
		{ u"action"_q, u"replace"_q },
		{ u"enabled"_q, enabled },
		{ u"caseInsensitive"_q, false },
		{ u"reversed"_q, false },
	};
}

QByteArray Bytes(const QJsonObject &value) {
	return QJsonDocument(value).toJson(QJsonDocument::Compact);
}

QByteArray Account(bool enabled, QJsonArray rules, QJsonArray excluded = {}) {
	auto config = Defaults();
	config.insert(u"enabled"_q, enabled);
	config.insert(u"rules"_q, rules);
	config.insert(u"excludedPeers"_q, excluded);
	return Bytes(config);
}

QByteArray Global(QJsonArray rules) {
	auto config = GlobalDefaults();
	config.insert(u"rules"_q, rules);
	return Bytes(config);
}

QJsonObject Place(
		const QString &peer,
		const QString &topic,
		const QString &state,
		QJsonArray rules = {},
		QJsonArray disabled = {}) {
	auto scope = NewScope(peer, topic);
	scope.insert(u"enabled"_q, state);
	scope.insert(u"rules"_q, rules);
	scope.insert(u"disabledRules"_q, disabled);
	return scope;
}

QByteArray Scopes(
		QJsonArray scopes,
		bool inheritGlobal = true,
		QJsonArray disabled = {}) {
	auto config = ScopeDefaults();
	config.insert(u"account"_q, QJsonObject{
		{ u"inheritGlobal"_q, inheritGlobal },
		{ u"disabledRules"_q, disabled },
	});
	config.insert(u"scopes"_q, scopes);
	return Bytes(config);
}

QString Run(
		const QByteArray &account,
		const QByteArray &global,
		const QByteArray &scopes,
		const QString &peer,
		const QString &topic,
		const QString &text) {
	const auto resolved = Resolve(account, global, scopes, peer, topic);
	Require(resolved.error.isEmpty(), "unexpected filter resolve error");
	const auto result = Apply(
		resolved.config, { text }, {}, peer, false, false);
	Require(result.error.isEmpty(), "unexpected filter apply error");
	return result.text.text;
}

void TestResolveStates() {
	const auto rule = QJsonArray{ Rule(1, u"a"_q, u"A"_q) };
	const auto on = Account(true, rule);
	const auto off = Account(false, rule);
	const auto peer = u"100"_q;

	Require(Resolve(on, {}, {}, peer, u"0"_q).config == on
		&& Resolve({}, {}, {}, peer, u"7"_q).config.isEmpty(),
		"empty inheritance options must return the account config as is");
	const auto broken = QByteArray("{\"version\":2}");
	Require(Resolve(broken, Global(rule), {}, peer, u"0"_q).config == broken,
		"an invalid account config must be passed through unchanged");

	const auto excluded = Account(true, rule, QJsonArray{ peer });
	const auto global = Global(QJsonArray{ Rule(2, u"b"_q, u"B"_q) });
	Require(Run(excluded, global, {}, peer, u"0"_q, u"ab"_q) == u"ab"_q,
		"excluded chat must override the account switch");
	Require(Run(excluded, global, {}, u"200"_q, u"0"_q, u"ab"_q) == u"AB"_q,
		"account switch must apply to chats that are not excluded");

	const auto chatOn = Scopes(QJsonArray{ Place(peer, u"0"_q, u"on"_q) });
	Require(Run(off, {}, chatOn, peer, u"0"_q, u"a"_q) == u"A"_q,
		"chat override must filter while the account switch is off");
	Require(Run(off, {}, chatOn, u"200"_q, u"0"_q, u"a"_q) == u"a"_q,
		"chat override leaked into another chat");
	Require(Run(excluded, {}, chatOn, peer, u"0"_q, u"a"_q) == u"A"_q,
		"explicit chat entry must win over the exclusion list");

	const auto mixed = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"off"_q),
		Place(peer, u"7"_q, u"on"_q),
	});
	Require(Run(on, {}, mixed, peer, u"7"_q, u"a"_q) == u"A"_q
		&& Run(on, {}, mixed, peer, u"8"_q, u"a"_q) == u"a"_q
		&& Run(on, {}, mixed, peer, u"0"_q, u"a"_q) == u"a"_q,
		"topic value must override the chat value for that topic only");
	const auto topicOff = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"on"_q),
		Place(peer, u"7"_q, u"off"_q),
	});
	Require(Run(off, {}, topicOff, peer, u"7"_q, u"a"_q) == u"a"_q
		&& Run(off, {}, topicOff, peer, u"8"_q, u"a"_q) == u"A"_q,
		"topic off must override chat on");

	const auto state = InheritedEnabled(off, topicOff, peer, u"7"_q);
	Require(state.enabled && state.layer == Layer::Chat,
		"topic must report the chat as its inherited source");
	const auto plain = InheritedEnabled(excluded, {}, peer, u"0"_q);
	Require(!plain.enabled && plain.layer == Layer::Account,
		"excluded chat must report filtering as off");
}

void TestResolveRules() {
	const auto peer = u"100"_q;
	const auto account = Account(true, QJsonArray{ Rule(2, u"x"_q, u"[A]"_q) });
	const auto global = Global(QJsonArray{ Rule(1, u"x"_q, u"[D]"_q) });
	const auto chat = QJsonArray{ Rule(3, u"y"_q, u"[C]"_q) };
	const auto topic = QJsonArray{ Rule(4, u"z"_q, u"[T]"_q) };
	const auto scopes = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"inherit"_q, chat),
		Place(peer, u"7"_q, u"inherit"_q, topic),
	});
	Require(Run(account, global, scopes, peer, u"7"_q, u"xyz"_q)
		== u"[D][C][T]"_q,
		"rules must run in global, account, chat, topic order");
	Require(Run(account, global, scopes, peer, u"0"_q, u"xyz"_q)
		== u"[D][C]z"_q,
		"topic rules leaked into the chat scope");
	Require(Run(account, global, scopes, u"200"_q, u"0"_q, u"xyz"_q)
		== u"[D]yz"_q,
		"chat rules leaked into another chat");

	const auto noGlobal = Scopes({}, false);
	Require(Run(account, global, noGlobal, peer, u"0"_q, u"x"_q) == u"[A]"_q,
		"inheritGlobal off must drop global rules");
	const auto accountDisables = Scopes({}, true, QJsonArray{ Id(1), Id(99) });
	Require(Run(account, global, accountDisables, peer, u"0"_q, u"x"_q)
		== u"[A]"_q,
		"account must be able to turn off one global rule");
	const auto chatDisables = Scopes(QJsonArray{ Place(
		peer, u"0"_q, u"inherit"_q, {}, QJsonArray{ Id(1), Id(2), Id(98) }) });
	Require(Run(account, global, chatDisables, peer, u"0"_q, u"x"_q) == u"x"_q
		&& Run(account, global, chatDisables, u"200"_q, u"0"_q, u"x"_q)
			== u"[D]"_q,
		"chat must turn off inherited rules for itself only");
	const auto topicDisables = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"inherit"_q, chat),
		Place(peer, u"7"_q, u"inherit"_q, {}, QJsonArray{ Id(3) }),
	});
	Require(Run(account, global, topicDisables, peer, u"7"_q, u"y"_q) == u"y"_q
		&& Run(account, global, topicDisables, peer, u"0"_q, u"y"_q)
			== u"[C]"_q,
		"topic must turn off a chat rule for itself only");

	const auto inherited = InheritedRules(
		account, global, scopes, peer, u"7"_q);
	Require(inherited.size() == 3
		&& inherited[0].layer == Layer::Global
		&& inherited[1].layer == Layer::Account
		&& inherited[2].layer == Layer::Chat,
		"inherited rule list must name the source layers");

	const auto restored = Scopes({});
	Require(Resolve(account, global, restored, peer, u"7"_q).config
		== Resolve(account, global, {}, peer, u"7"_q).config,
		"removing the entries must equal never having set them");
	const auto once = Resolve(account, global, scopes, peer, u"7"_q);
	Require(Validate(once.config)
		&& Resolve(account, global, scopes, peer, u"7"_q).config
			== once.config
		&& Resolve(once.config, {}, {}, peer, u"7"_q).config == once.config,
		"resolving must be stable and produce a valid v1 config");
}

void TestResolveLimits() {
	const auto peer = u"100"_q;
	auto many = QJsonArray();
	for (auto i = 0; i != 32; ++i) {
		many.push_back(Rule(100 + i, u"q%1"_q.arg(i), u"-"_q));
	}
	const auto account = Account(true, many);
	Require(Resolve(account, Global(QJsonArray{
		Rule(1, u"x"_q, u"X"_q, false),
	}), {}, peer, u"0"_q).error.isEmpty(),
		"32 effective rules must pass, disabled rules do not count");
	const auto over = Resolve(
		account,
		Global(QJsonArray{ Rule(1, u"x"_q, u"X"_q) }),
		{},
		peer,
		u"0"_q);
	Require(over.error == u"filter rule count limit"_q
		&& over.count == 33 && over.config.isEmpty(),
		"33 effective rules must be reported and must not filter");
	const auto relieved = Scopes(QJsonArray{ Place(
		peer, u"0"_q, u"inherit"_q, {}, QJsonArray{ Id(1) }) });
	Require(Resolve(account, Global(QJsonArray{ Rule(1, u"x"_q, u"X"_q) }),
		relieved, peer, u"0"_q).error.isEmpty(),
		"turning off an inherited rule must bring the scope under the limit");

	const auto clash = Resolve(
		Account(true, QJsonArray{ Rule(1, u"a"_q, u"A"_q) }),
		Global(QJsonArray{ Rule(1, u"b"_q, u"B"_q) }),
		{},
		peer,
		u"0"_q);
	Require(clash.error == u"filter rule id conflict"_q
		&& clash.config.isEmpty(),
		"a rule id shared between layers must be reported");
}

void TestScopeValidation() {
	const auto peer = u"100"_q;
	Require(ValidateScopes({}) && ValidateGlobal({})
		&& ValidateScopes(Bytes(ScopeDefaults()))
		&& ValidateGlobal(Bytes(GlobalDefaults())),
		"default inheritance configs rejected");
	const auto valid = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"on"_q, QJsonArray{ Rule(1, u"a"_q, u"A"_q) }),
		Place(peer, u"7"_q, u"off"_q),
	});
	Require(ValidateScopes(valid), "valid scope config rejected");

	const auto with = [&](const QString &key, const QJsonValue &value) {
		auto scope = Place(peer, u"0"_q, u"on"_q);
		scope.insert(key, value);
		return Scopes(QJsonArray{ scope });
	};
	auto unknownTop = ScopeDefaults();
	unknownTop.insert(u"extra"_q, 1);
	auto wrongVersion = ScopeDefaults();
	wrongVersion.insert(u"version"_q, 2);
	auto unknownOwn = ScopeDefaults();
	unknownOwn.insert(u"account"_q, QJsonObject{
		{ u"inheritGlobal"_q, true },
		{ u"disabledRules"_q, QJsonArray() },
		{ u"extra"_q, 1 },
	});
	auto tooMany = QJsonArray();
	for (auto i = 0; i != 201; ++i) {
		tooMany.push_back(Place(QString::number(1000 + i), u"0"_q, u"on"_q));
	}
	auto tooManyIds = QJsonArray();
	for (auto i = 0; i != 65; ++i) {
		tooManyIds.push_back(Id(i + 1));
	}
	auto heavy = QJsonArray();
	for (auto i = 0; i != 40; ++i) {
		heavy.push_back(Place(QString::number(1000 + i), u"0"_q, u"on"_q,
			QJsonArray{ Rule(500 + i, QString(2000, u'a'),
				QString(4000, u'b')) }));
	}
	const auto bad = std::vector<std::pair<QByteArray, const char*>>{
		{ "[]", "non-object scope config accepted" },
		{ "{", "truncated scope config accepted" },
		{ Bytes(wrongVersion), "scope config version 2 accepted" },
		{ Bytes(unknownTop), "unknown top-level scope field accepted" },
		{ Bytes(unknownOwn), "unknown account field accepted" },
		{ with(u"extra"_q, 1), "unknown scope field accepted" },
		{ with(u"enabled"_q, u"maybe"_q), "invalid enabled value accepted" },
		{ with(u"enabled"_q, true), "boolean enabled value accepted" },
		{ with(u"peer"_q, u"0100"_q), "non-canonical peer accepted" },
		{ with(u"peer"_q, u"0"_q), "zero peer accepted" },
		{ with(u"peer"_q, 100), "numeric peer accepted" },
		{ with(u"topic"_q, u"-1"_q), "negative topic accepted" },
		{ with(u"disabledRules"_q, QJsonArray{ u"x"_q }),
			"malformed disabled rule id accepted" },
		{ with(u"disabledRules"_q, tooManyIds),
			"65 disabled rule ids accepted" },
		{ with(u"rules"_q, QJsonArray{ Rule(1, u"("_q, u"A"_q) }),
			"scope rule with an invalid pattern accepted" },
		{ Scopes(QJsonArray{
			Place(peer, u"0"_q, u"on"_q),
			Place(peer, u"0"_q, u"off"_q),
		}), "duplicate scope accepted" },
		{ Scopes(QJsonArray{
			Place(peer, u"0"_q, u"on"_q, QJsonArray{ Rule(1, u"a"_q, {}) }),
			Place(peer, u"7"_q, u"on"_q, QJsonArray{ Rule(1, u"b"_q, {}) }),
		}), "duplicate rule id across scopes accepted" },
		{ Scopes(tooMany), "201 scopes accepted" },
		{ Scopes(heavy), "scope config above 128 KB accepted" },
	};
	for (const auto &[raw, message] : bad) {
		Require(!ValidateScopes(raw), message);
	}
	auto unknownGlobal = GlobalDefaults();
	unknownGlobal.insert(u"enabled"_q, true);
	auto manyGlobal = QJsonArray();
	for (auto i = 0; i != 33; ++i) {
		manyGlobal.push_back(Rule(i + 1, u"a"_q, u"A"_q));
	}
	Require(!ValidateGlobal("[]")
		&& !ValidateGlobal(Bytes(unknownGlobal))
		&& !ValidateGlobal(Global(manyGlobal))
		&& !ValidateGlobal(Global(QJsonArray{
			Rule(1, u"a"_q, {}),
			Rule(1, u"b"_q, {}),
		})),
		"invalid global rule config accepted");

	const auto account = Account(true, QJsonArray{ Rule(1, u"a"_q, u"A"_q) });
	Require(Run(account, {}, with(u"extra"_q, 1), peer, u"0"_q, u"a"_q)
		== u"A"_q
		&& Run(account, "[]", {}, peer, u"0"_q, u"a"_q) == u"A"_q,
		"account rules must keep working next to invalid override data");
}

void TestScopeOptions() {
	auto registry = Registry();
	RegisterOptions(registry);
	Require(registry.Find(kGlobalRules.key)
		&& registry.HasFlag(kGlobalRules.key, Flag::Exportable)
		&& registry.HasFlag(kGlobalRules.key, Flag::RefreshMessageView),
		"global filter rules must be an exportable device option");
	Require(registry.Find(kScopes.key)
		&& registry.Find(kScopes.key)->scope == Scope::Account
		&& !registry.HasFlag(kScopes.key, Flag::Exportable)
		&& registry.HasFlag(kScopes.key, Flag::RefreshMessageView),
		"filter scopes must stay in the account and out of exports");

	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	const auto one = Scopes(QJsonArray{ Place(u"100"_q, u"0"_q, u"on"_q) });
	const auto two = Scopes(QJsonArray{ Place(u"100"_q, u"0"_q, u"off"_q) });
	Require(first.Set(kScopes, one) && first.Get(kScopes) == one
		&& second.Get(kScopes).isEmpty(),
		"filter scopes leaked into another account");
	Require(second.Set(kScopes, two) && first.Get(kScopes) == one,
		"another account overwrote the filter scopes");
	Require(!first.Set(kScopes, QByteArray("{}")) && first.Get(kScopes) == one,
		"invalid filter scopes were written");
	firstPrefs.values[std::string(kScopes.key)] = "broken";
	Require(first.Get(kScopes).isEmpty()
		&& first.invalidKeys().contains(kScopes.key)
		&& firstPrefs.values[std::string(kScopes.key)] == "broken",
		"invalid filter scopes must fall back and stay stored");
}

void TestRegexBudget() {
	using Regex::Found;
	auto clock = QElapsedTimer();
	clock.start();
	Require(Regex::Find(Regex::Compile(u"(a+)+$"_q),
		QString(30, u'a') + u'!') == Found::Limit,
		"nested quantifier must stop at the match limit");
	Require(Regex::Find(Regex::Compile(u"(.*a){20}"_q),
		QString(4000, u'a') + QString(4000, u'b')) == Found::Limit,
		"repeated greedy group must stop at the match limit");
	Require(Regex::Find(Regex::Compile(u"^a+$"_q), u"aaa"_q) == Found::Yes
		&& Regex::Find(Regex::Compile(u"^a+$"_q), u"aab"_q) == Found::No,
		"plain patterns must report matches");
	Require(clock.elapsed() < 500, "regex limits exceeded the test duration");

	const auto intersection = Regex::Check(u"x[a-z&&[^aeiou]]"_q);
	Require(intersection && intersection->position == 5,
		"Java class intersection must be reported with its position");
	const auto property = Regex::Check(u"ab\\p{javaLowerCase}"_q);
	Require(property && property->position >= 2 && !property->text.isEmpty(),
		"Java character property must be reported with its position");
	Require(Regex::Check(u"(?d)a"_q).has_value(),
		"Java inline flag must be reported as unsupported");
	Require(!Regex::Check(u"[&]&&[\\]&]"_q)
		&& !Regex::Check(u"a&&b"_q)
		&& !Regex::Check(u"[]&]"_q),
		"literal ampersands outside an intersection must be accepted");

	const auto peer = u"100"_q;
	const auto slow = [](int index) {
		return QJsonArray{ Rule(index, u"(a+)+$"_q, u"-"_q) };
	};
	const auto scopes = Scopes(QJsonArray{
		Place(peer, u"0"_q, u"on"_q, slow(3)),
		Place(peer, u"7"_q, u"on"_q, slow(4)),
	});
	const auto resolved = Resolve(
		Account(true, slow(2)), Global(slow(1)), scopes, peer, u"7"_q);
	Require(resolved.error.isEmpty() && resolved.count == 4,
		"four slow rules must resolve");
	clock.restart();
	const auto text = QString(8000, u'a') + u'!';
	const auto result = Apply(
		resolved.config, { text }, {}, peer, false, false);
	Require(clock.elapsed() < 500,
		"slow rules in four layers exceeded the bounded test duration");
	Require(!result.error.isEmpty() && result.text.text == text,
		"slow rules must report the limit and keep the original text");
}

} // namespace

void TestFilterScopes() {
	TestResolveStates();
	TestResolveRules();
	TestResolveLimits();
	TestScopeValidation();
	TestScopeOptions();
	TestRegexBudget();
	std::cout << "PASS: Nagram filter inheritance and regex limits" << std::endl;
}
