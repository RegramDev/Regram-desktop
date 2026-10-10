#pragma once

#include <gsl/pointers>

namespace HistoryView {
class EmojiInteractions;
} // namespace HistoryView
namespace Stickers {
enum class EffectType : uint8;
} // namespace Stickers

namespace Nagram::Messages {

[[nodiscard]] bool EffectDisabled(Stickers::EffectType type);
[[nodiscard]] bool PremiumStickerEffectDisabled();

class Effects final {
public:
	static void Attach(not_null<HistoryView::EmojiInteractions*> interactions);
	[[nodiscard]] static bool ShouldProcessPending(
		not_null<HistoryView::EmojiInteractions*> interactions);

private:
	static void StopDisabled(
		not_null<HistoryView::EmojiInteractions*> interactions);
};

} // namespace Nagram::Messages
