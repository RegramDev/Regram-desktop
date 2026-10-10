#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Regram::Filters {

class RevealPolicy final {
public:
	[[nodiscard]] bool toggle(std::uint64_t peer, std::int64_t now);
	[[nodiscard]] bool active(std::uint64_t peer, std::int64_t now) const;
	[[nodiscard]] std::vector<std::uint64_t> expire(std::int64_t now);
	[[nodiscard]] std::optional<std::int64_t> nextExpiration() const;

private:
	std::map<std::uint64_t, std::int64_t> _deadlines;

};

} // namespace Regram::Filters
