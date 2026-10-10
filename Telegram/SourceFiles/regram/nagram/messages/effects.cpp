#include "regram/nagram/messages/effects.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "chat_helpers/stickers_emoji_pack.h"
#include "history/view/history_view_emoji_interactions.h"
#include "lottie/lottie_single_player.h"
#include "ui/rp_widget.h"

#include <algorithm>

namespace Nagram::Messages {

bool EffectDisabled(Stickers::EffectType type) {
	const auto &option = (type == Stickers::EffectType::PremiumSticker)
		? kDisablePremiumStickerEffects
		: (type == Stickers::EffectType::EmojiInteraction)
		? kDisableEmojiInteractions
		: kDisableMessageEffects;
	return ForDevice().Get(option);
}

bool PremiumStickerEffectDisabled() {
	return EffectDisabled(Stickers::EffectType::PremiumSticker);
}

void Effects::Attach(
		not_null<HistoryView::EmojiInteractions*> interactions) {
	ForDevice().changes(
	) | rpl::filter([](std::string_view key) {
		return key == kDisablePremiumStickerEffects.key
			|| key == kDisableEmojiInteractions.key
			|| key == kDisableMessageEffects.key;
	}) | rpl::on_next([interactions](std::string_view) {
		StopDisabled(interactions);
	}, interactions->_lifetime);
}

bool Effects::ShouldProcessPending(
		not_null<HistoryView::EmojiInteractions*> interactions) {
	if (!EffectDisabled(Stickers::EffectType::MessageEffect)) {
		return true;
	}
	interactions->_pendingEffects.clear();
	interactions->_downloadLifetime.destroy();
	return false;
}

void Effects::StopDisabled(
		not_null<HistoryView::EmojiInteractions*> interactions) {
	std::erase_if(interactions->_plays, [](const auto &play) {
		return EffectDisabled(play.type);
	});
	if (EffectDisabled(Stickers::EffectType::EmojiInteraction)) {
		interactions->_delayed.clear();
	}
	if (EffectDisabled(Stickers::EffectType::MessageEffect)) {
		interactions->_pendingEffects.clear();
		interactions->_downloadLifetime.destroy();
	}
	if (interactions->_layer) {
		interactions->_layer->update();
	}
}

} // namespace Nagram::Messages
