#include "regram/settings/shared.h"

#include "ayu/ayu_ui_settings.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/media/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/privacy/options.h"
#include "regram/settings/advanced_model.h"

#include <array>

namespace Regram::Settings {
namespace {

struct Binding {
	const Nagram::Option<bool> *option;
	bool (AyuSettings::*get)() const;
	void (AyuSettings::*set)(bool);
	bool inverted = false;
};

const auto kBindings = std::array{
	Binding{ &Nagram::Chats::kHideStories,
		&AyuSettings::disableStories, &AyuSettings::setDisableStories },
	Binding{ &Nagram::Chats::kHideAllChatsFolder,
		&AyuSettings::hideAllChatsFolder, &AyuSettings::setHideAllChatsFolder },
	Binding{ &Nagram::Messages::kSecondsInMessages,
		&AyuSettings::showMessageSeconds, &AyuSettings::setShowMessageSeconds },
	Binding{ &Nagram::Compose::kConfirmSticker,
		&AyuSettings::stickerConfirmation, &AyuSettings::setStickerConfirmation },
	Binding{ &Nagram::Compose::kConfirmGif,
		&AyuSettings::gifConfirmation, &AyuSettings::setGifConfirmation },
	Binding{ &Nagram::Interface::kHideAppIconBadge,
		&AyuSettings::hideNotificationBadge, &AyuSettings::setHideNotificationBadge },
	Binding{ &Nagram::Messages::kHidePrivateReactions,
		&AyuSettings::showPrivateChatReactions, &AyuSettings::setShowPrivateChatReactions, true },
	Binding{ &Nagram::Messages::kHideGroupReactions,
		&AyuSettings::showGroupReactions, &AyuSettings::setShowGroupReactions, true },
	Binding{ &Nagram::Messages::kHideChannelReactions,
		&AyuSettings::showChannelReactions, &AyuSettings::setShowChannelReactions, true },
	Binding{ &Nagram::Links::kWebAppAndroidPlatform,
		&AyuSettings::spoofWebviewAsAndroid, &AyuSettings::setSpoofWebviewAsAndroid },
	Binding{ &Nagram::Compose::kHideAttachButton,
		&AyuSettings::showAttachButtonInMessageField, &AyuSettings::setShowAttachButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideBotCommandButton,
		&AyuSettings::showCommandsButtonInMessageField, &AyuSettings::setShowCommandsButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideAutoDeleteButton,
		&AyuSettings::showAutoDeleteButtonInMessageField, &AyuSettings::setShowAutoDeleteButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideEmojiButton,
		&AyuSettings::showEmojiButtonInMessageField, &AyuSettings::setShowEmojiButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideRecordingButton,
		&AyuSettings::showMicrophoneButtonInMessageField, &AyuSettings::setShowMicrophoneButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideGiftButton,
		&AyuSettings::showGiftButtonInMessageField, &AyuSettings::setShowGiftButtonInMessageField, true },
	Binding{ &Nagram::Compose::kHideAiButton,
		&AyuSettings::showAiEditorButtonInMessageField, &AyuSettings::setShowAiEditorButtonInMessageField, true },
	Binding{ &Nagram::Compose::kDisableEmojiHover,
		&AyuSettings::showEmojiPopup, &AyuSettings::setShowEmojiPopup, true },
	Binding{ &Nagram::Media::kHideGreetingSticker,
		&AyuSettings::disableGreetingSticker, &AyuSettings::setDisableGreetingSticker },
	Binding{ &Nagram::Interface::kUniformAvatarShapes,
		&AyuSettings::singleCornerRadius, &AyuSettings::setSingleCornerRadius },
	Binding{ &Nagram::Chats::kHideSponsoredMessages,
		&AyuSettings::disableAds, &AyuSettings::setDisableAds },
	Binding{ &Nagram::Chats::kHideFolderUnreadCounters,
		&AyuSettings::hideNotificationCounters, &AyuSettings::setHideNotificationCounters },
	Binding{ &Nagram::Messages::kHideQuickShare,
		&AyuSettings::hideFastShare, &AyuSettings::setHideFastShare },
	Binding{ &Nagram::Messages::kHideRecommendedChannels,
		&AyuSettings::hideSimilarChannels, &AyuSettings::setHideSimilarChannels },
	Binding{ &Nagram::Messages::kCollapseRecommendedChannels,
		&AyuSettings::collapseSimilarChannels, &AyuSettings::setCollapseSimilarChannels },
	Binding{ &Nagram::Messages::kSimplifiedReplies,
		&AyuSettings::simpleQuotesAndReplies, &AyuSettings::setSimpleQuotesAndReplies },
	Binding{ &Nagram::Interface::kHideBubbleTail,
		&AyuSettings::removeMessageTail, &AyuSettings::setRemoveMessageTail },
};

} // namespace

void AttachSharedSettings(rpl::lifetime &lifetime) {
	if (!AyuSettings::StorageAvailable()) {
		return;
	}
	const auto pending = Nagram::ForDevice().Get(kAdvancedSettings);
	const auto snapshotValid = !Nagram::ForDevice().invalidKeys().contains(kAdvancedSettings.key);
	if (!pending.isEmpty()) {
		Expects(AyuSettings::ApplyPortableSettings(pending));
	}
	constexpr auto kMigratedKey = "regram.shared-settings-v3";
	auto &settings = Core::App().settings();
	if (settings.readPref<QByteArray>(kMigratedKey) != "1") {
		const auto radius = Nagram::ForDevice().Get(Nagram::Interface::kAvatarRoundness);
		if (radius) {
			AyuSettings::getInstance().setAvatarCorners(
				(radius * AyuUiSettings::kMaxAvatarCorners + 50) / 100);
			Expects(Nagram::ForDevice().Set(Nagram::Interface::kAvatarRoundness, 0));
		}
		if (settings.readPref<QByteArray>(Nagram::Privacy::kProfileIdFormat.key).isEmpty()) {
			const auto value = int(AyuSettings::getInstance().showPeerId());
			Expects(Nagram::ForDevice().Set(Nagram::Privacy::kProfileIdFormat,
				value ? 3 - value : 0));
		}
		if (AyuSettings::getInstance().disableNotificationsDelay()) {
			Expects(Nagram::ForDevice().Set(Nagram::Interface::kNotificationDelay, -1));
			Expects(Nagram::ForDevice().Set(Nagram::Interface::kOtherDeviceNotificationDelay, -1));
			AyuSettings::getInstance().setDisableNotificationsDelay(false);
		}
		auto index = 0;
		for (const auto &binding : kBindings) {
			if ((index >= 6 || AyuSettings::LegacySettingsLoaded())
				&& settings.readPref<QByteArray>(binding.option->key).isEmpty()) {
				Expects(Nagram::ForDevice().Set(*binding.option,
					(AyuSettings::getInstance().*binding.get)() != binding.inverted));
			}
			++index;
		}
		settings.writePref<QByteArray>(kMigratedKey, "1");
	}
	Nagram::ForDevice().Value(Nagram::Privacy::kProfileIdFormat)
	| rpl::on_next([](int value) {
		AyuSettings::getInstance().setShowPeerId(
			PeerIdDisplay(value ? 3 - value : 0));
	}, lifetime);
	AyuSettings::Changes() | rpl::on_next([] {
		const auto value = int(AyuSettings::getInstance().showPeerId());
		Expects(Nagram::ForDevice().Set(Nagram::Privacy::kProfileIdFormat,
			value ? 3 - value : 0));
		if (AyuSettings::getInstance().disableNotificationsDelay()) {
			AyuSettings::getInstance().setDisableNotificationsDelay(false);
			Expects(Nagram::ForDevice().Set(Nagram::Interface::kNotificationDelay, -1));
			Expects(Nagram::ForDevice().Set(Nagram::Interface::kOtherDeviceNotificationDelay, -1));
		}
	}, lifetime);
	for (const auto &binding : kBindings) {
		Nagram::ForDevice().Value(*binding.option)
		| rpl::on_next([binding](bool value) {
			(AyuSettings::getInstance().*binding.set)(value != binding.inverted);
		}, lifetime);
		AyuSettings::Changes()
		| rpl::on_next([binding] {
			Expects(Nagram::ForDevice().Set(*binding.option,
				(AyuSettings::getInstance().*binding.get)() != binding.inverted));
		}, lifetime);
	}
	if (!snapshotValid) {
		return;
	}
	const auto store = [](const QByteArray &raw) {
		if (!Nagram::ForDevice().Set(kAdvancedSettings, raw)) {
			Core::App().settings().writePref<QByteArray>(kAdvancedSettings.key, raw);
		}
	};
	const auto live = lifetime.make_state<QByteArray>(AyuSettings::PortableSettings());
	store(*live);
	AyuSettings::Changes() | rpl::on_next([=] {
		const auto updated = AyuSettings::PortableSettings();
		if (updated != *live) {
			*live = updated;
			store(updated);
		}
	}, lifetime);

}

} // namespace Regram::Settings
