#pragma once

#include "regram/nagram/core/options.h"

#include <vector>

namespace Nagram::Media {

inline constexpr auto kLocalFavedLimit = 200;

struct LocalFavedItem {
	quint64 id = 0;
	quint64 set = 0;
	quint64 hash = 0;
	int app = 0;
	QByteArray data;

	friend inline bool operator==(
		const LocalFavedItem&,
		const LocalFavedItem&) = default;
};

[[nodiscard]] bool ValidLocalFaved(const QByteArray &raw);
[[nodiscard]] std::vector<LocalFavedItem> ParseLocalFaved(
	const QByteArray &raw,
	quint64 user,
	int *skipped = nullptr);
[[nodiscard]] QByteArray SerializeLocalFaved(
	const std::vector<LocalFavedItem> &items,
	quint64 user);
[[nodiscard]] bool AddFavedItem(
	std::vector<LocalFavedItem> &items,
	LocalFavedItem item);
[[nodiscard]] bool RemoveFavedItem(
	std::vector<LocalFavedItem> &items,
	quint64 id);
[[nodiscard]] bool MergeFavedItems(
	std::vector<LocalFavedItem> &items,
	const std::vector<quint64> &serverIds);

inline constexpr auto kUnlimitedFavedStickers = Option<bool>{
	"nagram.unlimitedFavedStickers", Scope::Device, false,
	Category::Media, "lng_nagram_unlimited_faved_stickers" };
inline const auto kLocalFavedStickers = Option<QByteArray>{
	"nagram.localFavedStickers", Scope::Account, QByteArray(),
	Category::Media, "lng_nagram_local_faved_stickers",
	static_cast<unsigned>(Flag::Hidden), ValidLocalFaved };

inline void RegisterLocalFavedOptions(Registry &registry) {
	Expects(registry.Add(kUnlimitedFavedStickers));
	Expects(registry.Add(kLocalFavedStickers));
}

} // namespace Nagram::Media
