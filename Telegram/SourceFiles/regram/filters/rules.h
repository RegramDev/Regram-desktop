#pragma once

#include "regram/core/peer_address.h"

#include <memory>
#include <string>
#include <vector>

namespace Regram::Filters {

inline constexpr auto kMaxRules = 256;
inline constexpr auto kMaxPatternUnits = 2048;
inline constexpr auto kMaxTextUnits = 256 * 1024;
inline constexpr auto kMaxScopes = 1000;

struct Rule {
	std::string id;
	std::u16string pattern;
	bool regex = false;
	bool enabled = true;
	std::vector<PeerAddress> scopes;
	std::vector<std::string> unresolvedScopes;
};

struct Problem {
	std::string ruleId;
	std::string reason;
	int offset = -1;
};

struct Result {
	bool hidden = false;
	bool limited = false;
	std::string ruleId;
};

[[nodiscard]] bool SameRule(const Rule &a, const Rule &b);

class Engine final {
public:
	Engine();
	~Engine();
	Engine(const Engine &) = delete;
	Engine &operator=(const Engine &) = delete;

	[[nodiscard]] bool setRules(std::vector<Rule> rules);
	[[nodiscard]] const std::vector<Problem> &problems() const;
	[[nodiscard]] Result evaluate(
		std::u16string_view text,
		std::optional<PeerAddress> peer) const;

private:
	struct State;
	std::unique_ptr<State> _state;

};

} // namespace Regram::Filters
