#include "regram/filters/rules.h"

#include <unicode/normalizer2.h>
#include <unicode/regex.h>

#include <algorithm>
#include <chrono>
#include <set>

namespace Regram::Filters {
namespace {

constexpr auto kMatchTimeLimit = 10;
constexpr auto kStackLimit = 1024 * 1024;
constexpr auto kTotalTimeLimit = std::chrono::milliseconds(20);

struct CompiledRule {
	Rule rule;
	std::unique_ptr<icu::RegexPattern> pattern;
};

std::vector<PeerAddress> NormalizeScopes(std::vector<PeerAddress> scopes) {
	std::sort(scopes.begin(), scopes.end());
	scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());
	return scopes;
}

std::vector<std::string> NormalizeUnresolved(std::vector<std::string> scopes) {
	std::sort(scopes.begin(), scopes.end());
	scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());
	return scopes;
}

icu::UnicodeString ToUnicode(std::u16string_view text) {
	return icu::UnicodeString(text.data(), int32_t(text.size()));
}

bool AppliesTo(const Rule &rule, std::optional<PeerAddress> peer) {
	if (rule.scopes.empty()) {
		return true;
	} else if (!peer) {
		return false;
	}
	return std::any_of(
		rule.scopes.begin(),
		rule.scopes.end(),
		[=](PeerAddress scope) {
			return scope.kind == peer->kind
				&& scope.id == peer->id
				&& (!scope.thread || scope.thread == peer->thread);
		});
}

} // namespace

struct Engine::State {
	std::vector<CompiledRule> rules;
	std::vector<Problem> problems;
};

bool SameRule(const Rule &a, const Rule &b) {
	return a.pattern == b.pattern
		&& a.regex == b.regex
		&& NormalizeUnresolved(a.unresolvedScopes)
			== NormalizeUnresolved(b.unresolvedScopes)
		&& NormalizeScopes(a.scopes) == NormalizeScopes(b.scopes);
}

Engine::Engine() : _state(std::make_unique<State>()) {
}

Engine::~Engine() = default;

bool Engine::setRules(std::vector<Rule> rules) {
	if (rules.size() > kMaxRules) {
		return false;
	}
	auto scopeCount = size_t();
	for (const auto &rule : rules) {
		scopeCount += rule.scopes.size() + rule.unresolvedScopes.size();
		if (scopeCount > kMaxScopes) {
			return false;
		}
	}
	auto next = std::make_unique<State>();
	auto ids = std::set<std::string>();
	for (auto &rule : rules) {
		auto problem = Problem{ .ruleId = rule.id };
		if (rule.id.empty() || !ids.insert(rule.id).second) {
			problem.reason = "Duplicate or empty rule id";
		} else if (rule.pattern.empty()
			|| rule.pattern.size() > kMaxPatternUnits) {
			problem.reason = "Empty or oversized pattern";
		} else if (!rule.unresolvedScopes.empty()) {
			problem.reason = "Unresolved conversation scope";
		} else if (rule.scopes.size() > kMaxScopes
			|| std::any_of(rule.scopes.begin(), rule.scopes.end(), [](auto scope) {
				return !scope.id;
			})) {
			problem.reason = "Invalid conversation scope";
		}
		if (!problem.reason.empty()) {
			next->problems.push_back(std::move(problem));
			continue;
		}
		auto status = U_ZERO_ERROR;
		auto parse = UParseError();
		const auto flags = uint32_t(UREGEX_CASE_INSENSITIVE)
			| (rule.regex ? 0U : uint32_t(UREGEX_LITERAL));
		auto text = ToUnicode(rule.pattern);
		if (!rule.regex) {
			const auto normalizer = icu::Normalizer2::getNFCInstance(status);
			if (U_SUCCESS(status)) {
				auto normalized = icu::UnicodeString();
				normalizer->normalize(text, normalized, status);
				text = std::move(normalized);
			}
		}
		auto pattern = std::unique_ptr<icu::RegexPattern>(
			icu::RegexPattern::compile(
				text,
				flags,
				parse,
				status));
		if (U_FAILURE(status) || !pattern) {
			problem.reason = u_errorName(status);
			problem.offset = parse.offset;
			next->problems.push_back(std::move(problem));
			continue;
		}
		next->rules.push_back({ std::move(rule), std::move(pattern) });
	}
	_state = std::move(next);
	return true;
}

const std::vector<Problem> &Engine::problems() const {
	return _state->problems;
}

Result Engine::evaluate(
		std::u16string_view text,
		std::optional<PeerAddress> peer) const {
	if (text.empty() || _state->rules.empty()) {
		return {};
	} else if (text.size() > kMaxTextUnits) {
		return { .limited = true };
	}
	const auto deadline = std::chrono::steady_clock::now() + kTotalTimeLimit;
	const auto input = ToUnicode(text);
	auto normalized = icu::UnicodeString();
	auto normalizationStatus = U_ZERO_ERROR;
	const auto normalizer = icu::Normalizer2::getNFCInstance(normalizationStatus);
	if (U_SUCCESS(normalizationStatus)) {
		normalizer->normalize(input, normalized, normalizationStatus);
	}
	if (U_FAILURE(normalizationStatus)) {
		return { .limited = true };
	}
	auto result = Result();
	for (const auto &compiled : _state->rules) {
		if (!compiled.rule.enabled || !AppliesTo(compiled.rule, peer)) {
			continue;
		} else if (std::chrono::steady_clock::now() >= deadline) {
			result.limited = true;
			break;
		}
		auto status = U_ZERO_ERROR;
		auto matcher = std::unique_ptr<icu::RegexMatcher>(
			compiled.pattern->matcher(
				compiled.rule.regex ? input : normalized,
				status));
		if (U_FAILURE(status) || !matcher) {
			result.limited = true;
			continue;
		}
		matcher->setTimeLimit(kMatchTimeLimit, status);
		matcher->setStackLimit(kStackLimit, status);
		const auto found = matcher->find(status);
		if (U_FAILURE(status)) {
			result.limited = true;
		} else if (found) {
			result.hidden = true;
			result.ruleId = compiled.rule.id;
			break;
		}
	}
	return result;
}

} // namespace Regram::Filters
