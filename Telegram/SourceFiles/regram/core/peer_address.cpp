#include "regram/core/peer_address.h"

#include <bit>
#include <charconv>

namespace Regram {

std::optional<PeerAddress> FromIosPeerId(std::int64_t value) {
	const auto bits = std::bit_cast<std::uint64_t>(value);
	const auto kind = (bits >> 32) & 7;
	const auto id = (bits & 0xFFFFFFFFULL) | ((bits >> 35) << 32);
	if (kind > 2 || !id) {
		return std::nullopt;
	}
	return PeerAddress{
		.kind = static_cast<PeerKind>(kind),
		.id = id,
	};
}

std::optional<std::uint64_t> ParseServerId(
		std::string_view value,
		bool allowZero) {
	if (value.empty() || (value.size() > 1 && value.front() == '0')) {
		return std::nullopt;
	}
	auto result = std::uint64_t();
	const auto parsed = std::from_chars(
		value.data(),
		value.data() + value.size(),
		result);
	if (parsed.ec != std::errc()
		|| parsed.ptr != value.data() + value.size()
		|| (!allowZero && !result)) {
		return std::nullopt;
	}
	return result;
}

} // namespace Regram
