#include "regram/filters/reveal_policy.h"

#include "regram/filters/rules.h"

#include <limits>

namespace Regram::Filters {
namespace {

constexpr auto kDuration = std::int64_t(5 * 60 * 1000);

} // namespace

bool RevealPolicy::toggle(std::uint64_t peer, std::int64_t now) {
	if (!peer || now < 0 || now > std::numeric_limits<std::int64_t>::max() - kDuration) {
		return false;
	}
	if (active(peer, now)) {
		_deadlines.erase(peer);
		return true;
	} else if (!_deadlines.contains(peer) && _deadlines.size() == kMaxScopes) {
		return false;
	}
	_deadlines[peer] = now + kDuration;
	return true;
}

bool RevealPolicy::active(std::uint64_t peer, std::int64_t now) const {
	const auto found = _deadlines.find(peer);
	return found != _deadlines.end() && now < found->second;
}

std::vector<std::uint64_t> RevealPolicy::expire(std::int64_t now) {
	auto result = std::vector<std::uint64_t>();
	for (auto i = _deadlines.begin(); i != _deadlines.end();) {
		if (now >= i->second) {
			result.push_back(i->first);
			i = _deadlines.erase(i);
		} else {
			++i;
		}
	}
	return result;
}

std::optional<std::int64_t> RevealPolicy::nextExpiration() const {
	auto result = std::optional<std::int64_t>();
	for (const auto &[peer, until] : _deadlines) {
		if (!result || until < *result) {
			result = until;
		}
	}
	return result;
}

} // namespace Regram::Filters
