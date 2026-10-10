#include "regram/filters/exchange.h"
#include "regram/filters/scopes.h"

#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <cstdlib>
#include <iostream>

namespace {

void Check(bool passed, const char *name) {
	if (!passed) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit(1);
	}
}

void Legacy() {
	using namespace Regram;
	using namespace Regram::Filters;
	const auto imported = ImportRules(R"([
		{"id":"first","pattern":"广告","isRegex":false,"peerIds":[4294967297]},
		{"id":"second","pattern":"HELLO","isRegex":true,"peerIds":[9007199254740993]},
		{"id":"third","pattern":"secret","isRegex":false,"peerIds":[12884901889]},
		{"id":"negative","pattern":"signed","isRegex":false,"peerIds":[-9223372036854775808]}
	])");
	Check(imported.error.isEmpty(), "iOS rule array imported");
	Check(imported.rules.size() == 4, "all iOS rules retained");
	Check(imported.rules[0].scopes[0] == PeerAddress{ PeerKind::Group, 1 },
		"iOS group namespace decoded");
	Check(imported.rules[1].scopes[0] == PeerAddress{ PeerKind::User, 1125899906842625ULL },
		"numeric id greater than 2^53 remains exact");
	Check(imported.rules[3].scopes[0] == PeerAddress{ PeerKind::User, 1ULL << 60 },
		"negative packed iOS id remains exact");
	Check(!imported.rules[2].enabled && !imported.rules[2].unresolvedScopes.empty(),
		"secret conversation retained as disabled pending scope");
	const auto roundtrip = ImportRules(ExportRules(imported.rules));
	Check(roundtrip.error.isEmpty() && roundtrip.rules.size() == 4,
		"versioned export reimported");
	for (auto i = size_t(); i < imported.rules.size(); ++i) {
		Check(SameRule(imported.rules[i], roundtrip.rules[i]),
			"roundtrip preserves content and unresolved scopes");
	}
	const auto invalid = std::vector<QByteArray>{
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[1.1]}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[9007199254740993e0]}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[9223372036854775808]}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":["1"]}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":null}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[],"peerIds":[1]}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[],"\u0070eerIds":[1]}])",
		R"([{"id":"x","pattern":"\ud800","isRegex":false,"peerIds":[]}])",
		R"({"format":"regram.filters","version":2,"rules":[]})",
		"{",
	};
	for (const auto &data : invalid) {
		const auto result = ImportRules(data);
		Check(!result.error.isEmpty() && result.rules.empty(),
			"invalid import fails atomically");
	}
	const auto invalidRegex = ImportRules(
		R"([{"id":"x","pattern":"[","isRegex":true,"peerIds":[]}])");
	Check(invalidRegex.error.isEmpty() && !invalidRegex.rules[0].enabled
		&& !invalidRegex.problems.empty(), "invalid regex retained but disabled");
}

void Exchange() {
	using namespace Regram;
	using namespace Regram::Filters;
	const auto one = Rule{
		.id = "first",
		.pattern = u"你好\n👩‍💻 رسالة",
		.scopes = { { PeerKind::Channel, 9007199254740993ULL, 27 } },
	};
	const auto restored = ImportRules(ExportRules({ one }));
	Check(restored.error.isEmpty() && SameRule(one, restored.rules[0]),
		"Unicode and large string ids roundtrip");
	auto other = one;
	other.id = "other";
	other.scopes[0].id = 18;
	const auto merged = MergeRules({ one }, { one, other });
	Check(merged.added == 1 && merged.duplicates == 1 && merged.rules.size() == 2,
		"same content with a different scope remains distinct");
	Check(merged.rules[0].id == one.id && merged.rules[1].id != other.id,
		"existing ids preserved, new imports get fresh ids");
	const auto oversized = MergeRules(std::vector<Rule>(kMaxRules, one), { other });
	Check(!oversized.error.isEmpty() && oversized.rules.size() == kMaxRules
		&& oversized.added == 0, "oversized merge leaves current rules intact");
	auto object = QJsonDocument::fromJson(ExportRules({ one })).object();
	auto entries = object.value(u"rules"_q).toArray();
	auto entry = entries[0].toObject();
	entry.insert(u"scopes"_q, QJsonArray{ QJsonObject{
		{ u"kind"_q, u"future-peer-type"_q },
		{ u"id"_q, u"9007199254740993"_q },
		{ u"thread"_q, u"0"_q },
	} });
	entries[0] = entry;
	object.insert(u"rules"_q, entries);
	const auto futureScope = ImportRules(QJsonDocument(object).toJson());
	Check(futureScope.error.isEmpty() && !futureScope.rules[0].enabled
		&& futureScope.rules[0].scopes.empty()
		&& !futureScope.rules[0].unresolvedScopes.empty(),
		"future scope never broadens to all conversations");
}

} // namespace

int main() {
	const auto scoped = Regram::Filters::ParseScopes(
		u"user:9007199254740993\nchannel:123:456,USER:9007199254740993"_q);
	Check(scoped.error.isEmpty() && scoped.scopes.size() == 2,
		"scope entry precision, case and deduplication");
	const auto scopeCopy = Regram::Filters::ParseScopes(
		Regram::Filters::FormatScopes(scoped.scopes));
	Check(scopeCopy.error.isEmpty() && scopeCopy.scopes == scoped.scopes,
		"scope editor format preserves peer and topic IDs");
	for (const auto invalid : { u"user:0", u"secret:123", u"user:123:456",
		u"channel:123:0", u"user:1e5", u"group:-100123", u"user:123,secret:456" }) {
		const auto result = Regram::Filters::ParseScopes(QString::fromUtf16(invalid));
		Check(!result.error.isEmpty() && result.scopes.empty(),
			"invalid scope edits are rejected atomically");
	}
	Check(!Regram::Filters::ParseScopes(QString(64001, u' ')).error.isEmpty(),
		"scope list size is bounded before parsing");
	Legacy();
	Exchange();
	std::cout << "Regram rule exchange tests passed\n";
}
