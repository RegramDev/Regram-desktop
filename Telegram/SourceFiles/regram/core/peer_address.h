#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Regram {

enum class PeerKind {
	User,
	Group,
	Channel,
};

struct PeerAddress {
	PeerKind kind = PeerKind::User;
	std::uint64_t id = 0;
	std::uint64_t thread = 0;

	friend auto operator<=>(PeerAddress, PeerAddress) = default;
};

[[nodiscard]] std::optional<PeerAddress> FromIosPeerId(std::int64_t value);
[[nodiscard]] std::optional<std::uint64_t> ParseServerId(
	std::string_view value,
	bool allowZero = false);

} // namespace Regram
