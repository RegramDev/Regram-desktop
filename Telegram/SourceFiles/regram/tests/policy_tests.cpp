#include "regram/core/peer_address.h"
#include "regram/filters/rules.h"

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
	}
	Check(!FromIosPeerId(3ULL << 32 | 123), "secret peer stays unresolved");
	Check(!FromIosPeerId(0), "zero peer rejected");
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

void Filters() {
	using namespace Regram;
	using namespace Regram::Filters;
	auto engine = Engine();
	const auto peer = PeerAddress{ PeerKind::Channel, 123, 7 };
	auto rule = Rule{ .id = "rule", .pattern = u"广告", .scopes = { peer } };
	Check(engine.setRules({ rule }), "scoped rule accepted");
	Check(engine.evaluate(u"这是广告", peer).hidden, "matching topic hidden");
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
	std::cout << "Regram policy tests passed\n";
}
