#pragma once

#include "regram/nagram/core/options.h"

#include <vector>

namespace Nagram::Chats {

inline constexpr auto kLocalPinsLimit = 100;

// Below every server pin, above every date key and every custom sort key.
inline constexpr auto kLocalPinSortBase = 0xFFFFFFFE00000000ULL;

[[nodiscard]] constexpr quint64 LocalPinSortKeyFor(int index) {
	return kLocalPinSortBase + quint64(kLocalPinsLimit - index);
}

[[nodiscard]] bool ValidLocalPins(const QByteArray &raw);
[[nodiscard]] std::vector<quint64> ParseLocalPins(
	const QByteArray &raw,
	quint64 user);
[[nodiscard]] QByteArray SerializeLocalPins(
	const std::vector<quint64> &peers,
	quint64 user);
[[nodiscard]] bool AddLocalPin(std::vector<quint64> &peers, quint64 peer);
[[nodiscard]] bool RemoveLocalPin(std::vector<quint64> &peers, quint64 peer);
[[nodiscard]] bool MergeLocalPins(
	std::vector<quint64> &peers,
	const std::vector<quint64> &serverPinned);

inline constexpr auto kUnlimitedPinnedChats = Option<bool>{
	"nagram.unlimitedPinnedChats", Scope::Device, false,
	Category::Chats, "lng_nagram_unlimited_pinned_chats",
	static_cast<unsigned>(Flag::RefreshDialogList) };
inline const auto kLocalPinnedChats = Option<QByteArray>{
	"nagram.localPinnedChats", Scope::Account, QByteArray(),
	Category::Chats, "lng_nagram_local_pinned_chats",
	static_cast<unsigned>(Flag::Hidden), ValidLocalPins };

inline void RegisterLocalPinOptions(Registry &registry) {
	Expects(registry.Add(kUnlimitedPinnedChats));
	Expects(registry.Add(kLocalPinnedChats));
}

} // namespace Nagram::Chats
