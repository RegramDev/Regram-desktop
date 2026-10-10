#include "regram/filters/exchange.h"
#include "regram/filters/scopes.h"
#include "regram/filters/result_cache.h"

#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>

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
	const auto roundtrip = ImportStoredRules(ExportStoredRules(imported.rules));
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
		R"({"format":"regram.filters","version":3,"rules":[]})",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[],"isEnabled":"true"}])",
		R"([{"id":"x","pattern":"p","isRegex":false,"peerIds":[],"isException":1}])",
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
	const auto restored = ImportStoredRules(ExportStoredRules({ one }));
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
	auto object = QJsonDocument::fromJson(ExportStoredRules({ one })).object();
	auto entries = object.value(u"rules"_q).toArray();
	auto entry = entries[0].toObject();
	entry.insert(u"scopes"_q, QJsonArray{ QJsonObject{
		{ u"kind"_q, u"future-peer-type"_q },
		{ u"id"_q, u"9007199254740993"_q },
		{ u"thread"_q, u"0"_q },
	} });
	entries[0] = entry;
	object.insert(u"rules"_q, entries);
	const auto futureScope = ImportStoredRules(QJsonDocument(object).toJson());
	Check(futureScope.error.isEmpty() && !futureScope.rules[0].enabled
		&& futureScope.rules[0].scopes.empty()
		&& !futureScope.rules[0].unresolvedScopes.empty(),
		"future scope never broadens to all conversations");
}

void IosExchange() {
	using namespace Regram;
	using namespace Regram::Filters;
	const auto imported = ImportRules(R"json([
		{"id":"hide","pattern":"(?si)advert.*(sale)","isRegex":true,"peerIds":[],"isEnabled":true,"isException":false},
		{"id":"keep","pattern":"allowed","isRegex":false,"peerIds":[8589934634],"isEnabled":true,"isException":true},
		{"id":"off","pattern":"emoji 👩‍💻","isRegex":false,"peerIds":[-9223372036854775807],"isEnabled":false,"isException":false}
	])json");
	Check(imported.error.isEmpty() && imported.problems.empty()
		&& imported.rules.size() == 3, "current iOS six-field schema imports");
	Check(imported.rules[1].exception && !imported.rules[2].enabled,
		"iOS keep action and disabled flag survive import");
	const auto exported = ExportIosRules(imported.rules);
	Check(exported.error.isEmpty(), "iOS export succeeds for conversation scopes");
	const auto restored = ImportRules(exported.data);
	Check(restored.error.isEmpty() && restored.rules.size() == 3,
		"iOS export is a readable rule array");
	for (auto i = size_t(); i != imported.rules.size(); ++i) {
		Check(SameRule(imported.rules[i], restored.rules[i])
			&& imported.rules[i].enabled == restored.rules[i].enabled,
			"iOS roundtrip preserves exact scopes, action and state");
	}
	auto extended = imported.rules[1];
	extended.scopes[0].thread = 12;
	Check(!ExportIosRules({ extended }).error.isEmpty(),
		"iOS export cannot silently broaden a topic scope");
	extended.unresolvedScopes.push_back("future:42");
	Check(!ExportIosRules({ extended }).error.isEmpty(),
		"iOS export cannot silently drop unresolved scopes");
	Check(!ImportRules(ExportStoredRules(imported.rules)).error.isEmpty(),
		"public import rejects retired Desktop backup format");
	Check(ImportStoredRules(ExportStoredRules({ extended })).rules[0].enabled == false,
		"retired topic rules remain disabled and do not broaden on upgrade");

	const auto desktop = ImportStoredRules(ExportStoredRules(imported.rules));
	Check(desktop.error.isEmpty() && desktop.rules[1].exception,
		"desktop v2 backup preserves keep rules");
	const auto old = ImportStoredRules(R"({"format":"regram.filters","version":1,"rules":[{"id":"old","pattern":"hello","regex":false,"enabled":false,"scopes":[],"unresolvedScopes":[]}]})");
	Check(old.error.isEmpty() && !old.rules[0].enabled && !old.rules[0].exception,
		"previous Desktop backups remain readable");
	auto hide = imported.rules[1];
	hide.exception = false;
	const auto merged = MergeRules(imported.rules, { hide, imported.rules[1] });
	Check(merged.added == 1 && merged.duplicates == 1,
		"merge distinguishes hide from keep and deduplicates definitions");
	Check(IsRulesFileName(u"Regram-message-filter 2.JSON"_q)
		&& !IsRulesFileName(u"regram-filters.json"_q)
		&& !IsRulesFileName(u"notes.json"_q)
		&& !IsRulesFileName(u"regram-message-filter.json.exe"_q),
		"chat import recognizes only iOS names and numbered copies");
}

void ResultCache() {
	using namespace Regram::Filters;
	auto first = base::has_weak_ptr();
	auto second = base::has_weak_ptr();
	auto cache = Regram::Filters::ResultCache(3 * 1024);
	const auto key = MessageKey{ 1, 42, 7 };
	cache.put(key, &first, u"spam"_q, true);
	cache.put({ 2, 42, 7 }, &second, u"safe"_q, false);
	Check(cache.get(key, &first, u"spam"_q) == true
		&& cache.get({ 2, 42, 7 }, &second, u"safe"_q) == false,
		"filter verdicts remain isolated by account");
	Check(!cache.get(key, &second, u"spam"_q)
		&& !cache.get(key, &first, u"edited"_q),
		"stale sessions and edited messages cannot reuse filter verdicts");
	Check(cache.hasHidden(1, 42, &first) && !cache.hasHidden(2, 42, &second),
		"filtered-message indicator is account scoped");
	for (auto i = 0; i != 100000; ++i) {
		cache.put({ 1, 42, i + 100 }, &first, u"message"_q, false);
	}
	Check(cache.totalCost() <= 3 * 1024 && !cache.get(key, &first, u"spam"_q),
		"long histories keep a bounded filter verdict cache");
	cache.put(key, &first, u"spam"_q, true);
	invalidate_weak_ptrs(&first);
	Check(!cache.get(key, &first, u"spam"_q),
		"logging out invalidates cached results even at the same owner address");
}

} // namespace

int main(int argc, char *argv[]) {
	const auto scoped = Regram::Filters::ParseScopes(
		u"user:9007199254740993\nchannel:123,USER:9007199254740993"_q);
	Check(scoped.error.isEmpty() && scoped.scopes.size() == 2,
		"scope entry precision, case and deduplication");
	const auto scopeCopy = Regram::Filters::ParseScopes(
		Regram::Filters::FormatScopes(scoped.scopes));
	Check(scopeCopy.error.isEmpty() && scopeCopy.scopes == scoped.scopes,
		"scope editor format preserves exact conversation IDs");
	for (const auto invalid : { u"user:0", u"secret:123", u"user:123:456",
		u"channel:123:0", u"channel:123:456", u"user:1e5", u"group:-100123", u"user:123,secret:456" }) {
		const auto result = Regram::Filters::ParseScopes(QString::fromUtf16(invalid));
		Check(!result.error.isEmpty() && result.scopes.empty(),
			"invalid scope edits are rejected atomically");
	}
	Check(!Regram::Filters::ParseScopes(QString(64001, u' ')).error.isEmpty(),
		"scope list size is bounded before parsing");
	Legacy();
	Exchange();
	IosExchange();
	ResultCache();
	if (argc == 2) {
		auto file = QFile(QString::fromLocal8Bit(argv[1]));
		Check(file.open(QIODevice::ReadOnly), "external compatibility fixture opens");
		const auto imported = Regram::Filters::ImportRules(file.readAll());
		Check(imported.error.isEmpty() && imported.problems.empty(),
			"external fixture imports without invalid or disabled-by-error rules");
		const auto exported = Regram::Filters::ExportIosRules(imported.rules);
		Check(exported.error.isEmpty(), "external fixture exports for iOS");
		const auto restored = Regram::Filters::ImportRules(exported.data);
		Check(restored.error.isEmpty() && restored.rules.size() == imported.rules.size(),
			"external fixture roundtrip preserves rule count");
		std::cout << "External fixture: " << imported.rules.size() << " rules passed\n";
	}
	std::cout << "Regram rule exchange tests passed\n";
}
