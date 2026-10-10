#include "regram/core/peer_address.h"
#include "regram/filters/rules.h"
#include "regram/filters/matcher.h"
#include "regram/filters/reveal_policy.h"

#include <bit>
#include <cstdlib>
#include <iostream>

namespace {

void Check(bool passed, const char *name) {
	if (!passed) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit(1);
	}
}

void PeerIds() {
	using namespace Regram;
	const auto cases = std::vector<PeerAddress>{
		{ PeerKind::User, 4294967297ULL },
		{ PeerKind::Group, 1234567ULL },
		{ PeerKind::Channel, 9876543210ULL },
		{ PeerKind::User, (1ULL << 60) + 41 },
	};
	for (const auto peer : cases) {
		const auto packed = (peer.id & 0xFFFFFFFFULL)
			| (uint64_t(peer.kind) << 32)
			| ((peer.id >> 32) << 35);
		Check(FromIosPeerId(std::bit_cast<int64_t>(packed)) == peer,
			"iOS namespaces, high bits and signed encoding");
		Check(ToIosPeerId(peer) == std::bit_cast<int64_t>(packed),
			"iOS peer export retains every bit");
	}
	Check(!FromIosPeerId(3ULL << 32 | 123), "secret peer stays unresolved");
	Check(!FromIosPeerId(0), "zero peer rejected");
	Check(!ToIosPeerId({ PeerKind::User, 1ULL << 61 }),
		"unrepresentable iOS id rejected");
	Check(!ToIosPeerId({ PeerKind::Channel, 1, 12 }),
		"iOS cannot encode topic scope");
	Check(ParseServerId("9007199254740993") == 9007199254740993ULL,
		"64-bit precision beyond JSON double");
	Check(ParseServerId("18446744073709551615") == UINT64_MAX,
		"maximum portable id");
	Check(!ParseServerId("18446744073709551616"), "overflow rejected");
	for (const auto invalid : { "", "00", "01", "+1", "-1", "1.0", "1x", " 1" }) {
		Check(!ParseServerId(invalid), "noncanonical id rejected");
	}
	Check(ParseServerId("0", true) == 0, "zero topic allowed");
}

void Exceptions() {
	using namespace Regram;
	using namespace Regram::Filters;
	auto engine = Engine();
	const auto peer = PeerAddress{ PeerKind::Channel, 42 };
	const auto hide = Rule{ .id = "hide", .pattern = u"sale" };
	auto keep = Rule{
		.id = "keep",
		.pattern = u"approved",
		.exception = true,
		.scopes = { peer },
	};
	Check(engine.setRules({ hide, keep }), "hide followed by keep accepted");
	Check(!engine.evaluate(u"approved SALE", peer).hidden,
		"keep rule wins regardless of rule order");
	Check(engine.evaluate(u"approved sale", PeerAddress{ PeerKind::User, 42 }).hidden,
		"keep applies only to its conversation namespace");
	Check(engine.evaluate(u"sale", peer).hidden, "unmatched keep does not suppress hide");
	keep.enabled = false;
	Check(engine.setRules({ hide, keep }), "disabled keep accepted");
	Check(engine.evaluate(u"approved sale", peer).hidden, "disabled keep is inert");
	keep.enabled = true;
	keep.regex = true;
	keep.pattern = u"(a+)+$";
	Check(engine.setRules({ hide, keep }), "slow keep accepted");
	auto text = std::u16string(100000, u'a');
	text += u"! sale";
	const auto result = engine.evaluate(text, peer);
	Check(result.limited && !result.hidden,
		"timed-out keep cannot allow a later hide to remove the message");
}

void TimeoutRecovery() {
	using namespace Regram::Filters;
	auto rules = std::vector<Rule>();
	for (auto index = 0; index != 8; ++index) {
		rules.push_back({
			.id = std::to_string(index),
			.pattern = u"(a+)+$",
			.regex = true,
		});
	}
	rules.push_back({ .id = "literal", .pattern = u"sale" });
	auto engine = Engine();
	Check(engine.setRules(rules), "slow regex set accepted");
	auto text = std::u16string(100000, u'a') + u"! sale";
	Check(engine.evaluate(text, std::nullopt).hidden,
		"iOS timeout budget continues to check literal rules");
}

void Reveal() {
	using namespace Regram::Filters;
	auto first = RevealPolicy();
	auto otherAccount = RevealPolicy();
	Check(first.toggle(42, 1000), "temporary reveal begins");
	Check(first.active(42, 300999) && !first.active(42, 301000),
		"temporary reveal expires at exactly five minutes");
	Check(!otherAccount.active(42, 1000) && !first.active(43, 1000),
		"temporary reveal stays isolated by account and chat");
	Check(first.nextExpiration() == 301000, "timer follows the next expiration");
	Check(first.toggle(43, 2000), "second chat reveal begins");
	const auto expired = first.expire(301000);
	Check(expired == std::vector<std::uint64_t>{42} && first.active(43, 301000),
		"expiring one chat preserves another chat's reveal");
	Check(first.toggle(43, 301001) && !first.active(43, 301001)
		&& !first.nextExpiration(), "manual hide cancels the remaining timer");
	Check(!first.toggle(0, 0) && !first.toggle(42, -1), "invalid reveal key or clock rejected");
}

void Filters() {
	using namespace Regram;
	using namespace Regram::Filters;
	auto engine = Engine();
	const auto peer = PeerAddress{ PeerKind::Channel, 123, 7 };
	auto rule = Rule{ .id = "rule", .pattern = u"广告", .scopes = { peer } };
	Check(engine.setRules({ rule }), "retired topic rule retained for recovery");
	Check(!engine.evaluate(u"这是广告", peer).hidden && !engine.problems().empty(),
		"retired topic scope is inert without expanding to the whole chat");
	Check(!engine.evaluate(u"广告", PeerAddress{ PeerKind::Channel, 123, 8 }).hidden,
		"other topic preserved");
	Check(!engine.evaluate(u"广告", PeerAddress{ PeerKind::User, 123, 7 }).hidden,
		"peer type is part of scope");
	Check(!engine.evaluate(u"广告", std::nullopt).hidden, "unknown peer preserved");
	rule.scopes[0].thread = 0;
	Check(engine.setRules({ rule }), "conversation scope accepted");
	Check(engine.evaluate(u"广告", peer).hidden, "conversation includes topics");
	rule.unresolvedScopes = { "ios:unknown:42" };
	Check(engine.setRules({ rule }), "unresolved rule retained for diagnosis");
	Check(!engine.problems().empty(), "unresolved scope reported");
	Check(!engine.evaluate(u"广告", peer).hidden, "unresolved scope is inert");
	const auto samples = std::vector<std::pair<std::u16string, std::u16string>>{
		{ u"广告", u"这是广告" },
		{ u"hello", u"HELLO" },
		{ u"тест", u"ТЕСТ" },
		{ u"رسالة", u"هذه رسالة" },
		{ u"👩‍💻", u"你好👩‍💻" },
		{ u"café", u"CAFE\u0301" },
		{ u"strasse", u"Straße" },
		{ u"a.b", u"a.b" },
	};
	for (const auto &[pattern, text] : samples) {
		Check(engine.setRules({ { .id = "literal", .pattern = pattern } }),
			"Unicode literal accepted");
		Check(engine.evaluate(text, std::nullopt).hidden, "Unicode literal match");
	}
	Check(engine.setRules({ { .id = "literal", .pattern = u"a.b" } }),
		"literal metacharacters accepted");
	Check(!engine.evaluate(u"axb", peer).hidden, "literal regex symbols escaped");
	Check(engine.setRules({ { .id = "regex", .pattern = u"[a-z&&[^aeiou]]+", .regex = true } }),
		"ICU class intersection accepted");
	Check(engine.evaluate(u"BCD", peer).hidden, "ICU class intersection matches");
	Check(!engine.evaluate(u"aeiou", peer).hidden, "ICU class intersection excludes vowels");
	Check(engine.setRules({ { .id = "bad", .pattern = u"[", .regex = true } }),
		"invalid regex retained for diagnosis");
	Check(engine.problems().size() == 1, "invalid regex reported");
	Check(!engine.evaluate(u"anything", peer).hidden, "invalid regex preserves message");
	Check(engine.setRules({ { .id = "expensive", .pattern = u"(a+)+$", .regex = true } }),
		"complex regex accepted with budgets");
	auto text = std::u16string(100000, u'a');
	text.push_back(u'!');
	const auto limited = engine.evaluate(text, peer);
	Check(!limited.hidden && limited.limited, "backtracking budget preserves message");
	auto status = U_ZERO_ERROR;
	const auto negative = std::unique_ptr<icu::RegexPattern>(icu::RegexPattern::compile(
		icu::UnicodeString(u"(a+)+$"), 0, status));
	Check(U_SUCCESS(status) && negative, "legacy reverse pattern compiles");
	Check(MatchPattern(*negative, icu::UnicodeString(text.data(), int32_t(text.size())))
		== Match::Limited, "legacy inverted filter cannot interpret a timeout as a negative match");
	Check(engine.evaluate(std::u16string(kMaxTextUnits + 1, u'a'), peer).limited,
		"oversized text budget");
	Check(engine.setRules({ { .id = "retained", .pattern = u"hello" } }),
		"replacement after invalid rules");
	Check(!engine.setRules(std::vector<Rule>(kMaxRules + 1)), "oversized rule set rejected");
	Check(engine.evaluate(u"hello", peer).hidden, "failed replacement keeps previous rules");
	auto different = rule;
	different.scopes = { { PeerKind::User, 123 } };
	Check(!SameRule(rule, different), "deduplication respects scope");
	Check(SameRule(rule, rule), "identical rule deduplication");
}

} // namespace

int main() {
	PeerIds();
	Filters();
	Exceptions();
	TimeoutRecovery();
	Reveal();
	std::cout << "Regram policy tests passed\n";
}
