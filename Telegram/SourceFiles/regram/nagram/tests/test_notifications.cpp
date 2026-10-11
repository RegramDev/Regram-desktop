#include "regram/nagram/notifications/model.h"

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestNotifications() {
	using namespace Nagram;
	using namespace Nagram::Notifications;

	Require(ValidPolicy(u"default"_q) && ValidPolicy(u"silenced"_q)
		&& ValidPolicy(u"disabled"_q) && !ValidPolicy(u"silent"_q),
		"notification policy accepted an unknown action");
	Require(ValidPolicyExceptions(R"({"42":"default","43":"disabled"})")
		&& !ValidPolicyExceptions(R"({"042":"disabled"})")
		&& !ValidPolicyExceptions(R"({"42":true})")
		&& !ValidPolicyExceptions(R"({"42":"unknown"})"),
		"invalid notification exception accepted");
	Require(PolicyForPeer(u"silenced"_q, R"({"42":"default","43":"disabled"})", u"42"_q) == u"default"_q
		&& PolicyForPeer(u"silenced"_q, R"({"43":"disabled"})", u"43"_q) == u"disabled"_q
		&& PolicyForPeer(u"silenced"_q, QByteArray(), u"44"_q) == u"silenced"_q,
		"per-chat policies did not override the global choice");

	auto night = QuietHours{ .enabled = true };
	Require(Active(night, 1, 23 * 60)
		&& Active(night, 2, 0)
		&& Active(night, 2, 7 * 60 - 1)
		&& !Active(night, 2, 7 * 60)
		&& !Active(night, 2, 23 * 60 - 1),
		"quiet hours cross midnight");
	night.weekdays = { 5 };
	Require(Active(night, 5, 23 * 60 + 30)
		&& Active(night, 6, 6 * 60)
		&& !Active(night, 5, 6 * 60)
		&& !Active(night, 6, 23 * 60 + 30),
		"a night belongs to the day it starts on");
	night.weekdays = { 7 };
	Require(Active(night, 1, 60) && !Active(night, 7, 60),
		"Sunday night ends on Monday");
	auto day = QuietHours{
		.enabled = true,
		.start = 9 * 60,
		.end = 17 * 60,
		.weekdays = { 1, 2 },
	};
	Require(Active(day, 1, 9 * 60)
		&& !Active(day, 1, 17 * 60)
		&& !Active(day, 3, 12 * 60),
		"quiet hours inside a day");
	day.end = day.start;
	Require(Active(day, 2, 0) && Active(day, 2, 1439) && !Active(day, 3, 0),
		"equal times cover the whole day");
	day.enabled = false;
	Require(!Active(day, 2, 0), "disabled quiet hours never apply");

	auto quiet = QuietHours{ .enabled = true, .start = 0, .end = 0 };
	Require(Silences(quiet, { .message = true }, 1, 0)
		&& Silences(quiet, {}, 1, 0)
		&& Silences(quiet, {
			.message = true,
			.contact = true,
			.pinned = true,
			.mention = true,
			.keyword = true,
		}, 1, 0), "quiet hours silence everything without exceptions");
	quiet.allowContacts = true;
	quiet.allowKeywords = true;
	Require(!Silences(quiet, { .message = true, .contact = true }, 1, 0)
		&& !Silences(quiet, { .message = true, .keyword = true }, 1, 0)
		&& Silences(quiet, { .message = true, .pinned = true }, 1, 0)
		&& Silences(quiet, { .contact = true }, 1, 0),
		"quiet hours exceptions apply to messages only");

	Require(ParseQuietHours({}) == QuietHours()
		&& Serialize(QuietHours()).isEmpty(),
		"default quiet hours are stored as nothing");
	auto stored = QuietHours{
		.enabled = true,
		.start = 60,
		.end = 120,
		.weekdays = { 7, 1, 1 },
		.allowMentions = true,
	};
	const auto raw = Serialize(stored);
	stored.weekdays = { 1, 7 };
	Require(ParseQuietHours(raw) == stored && ValidQuietHours(raw),
		"quiet hours round trip with sorted weekdays");
	const auto quietJson = [](const char *start, const char *days) {
		return QByteArray(R"({"version":1,"enabled":true,"start":)") + start
			+ R"(,"end":0,"weekdays":)" + days
			+ R"(,"allowContacts":false,"allowPinned":false,)"
			+ R"("allowMentions":false,"allowKeywords":false})";
	};
	Require(ValidQuietHours(quietJson("1439", "[1,7]"))
		&& !ValidQuietHours(quietJson("1440", "[]"))
		&& !ValidQuietHours(quietJson("-1", "[]"))
		&& !ValidQuietHours(quietJson("1.5", "[]"))
		&& !ValidQuietHours(quietJson("0", "[0]"))
		&& !ValidQuietHours(quietJson("0", "[8]"))
		&& !ValidQuietHours(quietJson("0", "[2,2]"))
		&& !ValidQuietHours(quietJson("0", "[3,1]"))
		&& !ValidQuietHours(R"({"version":2})")
		&& !ValidQuietHours(R"({"version":1,"enabled":true})")
		&& !ValidQuietHours("[]"),
		"quiet hours reject values outside their ranges");
	Require(ParseMinute(u"7:05"_q) == 425
		&& ParseMinute(u" 23:59 "_q) == 1439
		&& !ParseMinute(u"24:00"_q)
		&& !ParseMinute(u"7:60"_q)
		&& !ParseMinute(u"7:5"_q)
		&& !ParseMinute(u"-1:00"_q)
		&& !ParseMinute(u"7"_q)
		&& FormatMinute(425) == u"07:05"_q,
		"quiet hours time text");

	const auto rules = ParseKeywordLines(
		u" urgent \n\n/re(lease|boot)/\n/Hotfix/i\nurgent\n/\n//\n/i"_q);
	Require(rules == std::vector<KeywordRule>{
		{ u"urgent"_q },
		{ u"re(lease|boot)"_q, true, true },
		{ u"Hotfix"_q, true, false },
		{ u"/"_q },
		{ u"//"_q },
		{ u"/i"_q },
	}, "keyword lines parsed without duplicates");
	Require(ParseKeywordLines(FormatKeywordLines(rules)) == rules,
		"keyword lines round trip");
	auto alerts = KeywordAlerts{ .enabled = true, .rules = rules };
	Require(Matches(alerts, u"This is URGENT!"_q, false)
		&& Matches(alerts, u"next release"_q, false)
		&& !Matches(alerts, u"next RELEASE"_q, false)
		&& Matches(alerts, u"a hotFIX"_q, false)
		&& !Matches(alerts, u"nothing here"_q, false)
		&& !Matches(alerts, QString(), false),
		"keywords match by case rules");
	Require(!Matches(alerts, u"urgent"_q, true), "channels need their switch");
	alerts.channels = true;
	Require(Matches(alerts, u"urgent"_q, true), "channels match when allowed");
	alerts.enabled = false;
	Require(!Matches(alerts, u"urgent"_q, false), "disabled alerts never match");

	auto slow = KeywordAlerts{
		.enabled = true,
		.rules = { { u"(*LIMIT_MATCH=100000000)(a+)+$"_q, true, true } },
	};
	Require(!Matches(slow, QString(40, u'a') + u'!', false),
		"a pattern cannot raise the match limit");
	Require(!Matches(alerts = KeywordAlerts{
			.enabled = true,
			.rules = { { u"needle"_q } },
		}, QString(20000, u'x') + u"needle"_q, false),
		"only the start of a long text is searched");

	const auto stored2 = KeywordAlerts{ true, true, rules };
	Require(ParseKeywordAlerts(Serialize(stored2)) == stored2
		&& Serialize(KeywordAlerts()).isEmpty()
		&& ParseKeywordAlerts({}) == KeywordAlerts(),
		"keyword alerts round trip");
	const auto alertsJson = [](const QByteArray &list) {
		return R"({"version":1,"enabled":true,"channels":false,"rules":[)"
			+ list + "]}";
	};
	const auto rule = [](const QByteArray &pattern, bool regex = false) {
		return R"({"pattern":")" + pattern + R"(","regex":)"
			+ (regex ? "true" : "false") + R"(,"caseSensitive":false})";
	};
	auto many = QByteArray();
	for (auto i = 0; i != kMaxKeywordRules + 1; ++i) {
		many += (i ? "," : "") + rule("k" + QByteArray::number(i));
	}
	Require(ValidKeywordAlerts(alertsJson(rule("a") + "," + rule("b", true)))
		&& !ValidKeywordAlerts(alertsJson(rule("a") + "," + rule("a")))
		&& !ValidKeywordAlerts(alertsJson(rule("  ")))
		&& !ValidKeywordAlerts(alertsJson(rule("(", true)))
		&& ValidKeywordAlerts(alertsJson(rule("(")))
		&& !ValidKeywordAlerts(alertsJson(rule(QByteArray(257, 'x'))))
		&& ValidKeywordAlerts(alertsJson(rule(QByteArray(256, 'x'))))
		&& !ValidKeywordAlerts(alertsJson(rule("a\\nb")))
		&& !ValidKeywordAlerts(alertsJson(many))
		&& !ValidKeywordAlerts(alertsJson(R"({"pattern":"a"})"))
		&& !ValidKeywordAlerts(R"({"version":1,"enabled":true})"),
		"keyword alerts reject bad rules");
	const auto problem = CheckKeywordRules({
		{ u"fine"_q },
		{ u"(broken"_q, true, true },
	});
	const auto tooLong = CheckKeywordRules({ { QString(257, u'x') } });
	Require(problem && problem->line == 2 && !problem->text.isEmpty()
		&& !CheckKeywordRules(rules)
		&& tooLong && tooLong->line == 1,
		"keyword problems name their line");

	auto registry = Registry();
	RegisterOptions(registry);
	Require(kQuietHours.scope == Scope::Device
		&& kKeywordAlerts.scope == Scope::Account
		&& registry.HasFlag(kQuietHours.key, Flag::Exportable)
		&& !registry.HasFlag(kKeywordAlerts.key, Flag::Exportable),
		"notification option scopes");
	std::cout << "PASS: Nagram quiet hours and keyword alerts" << std::endl;
}
