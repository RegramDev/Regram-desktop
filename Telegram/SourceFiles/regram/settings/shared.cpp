#include "regram/settings/shared.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/settings/advanced_model.h"

#include <array>

namespace Regram::Settings {
namespace {

struct Binding {
	const Nagram::Option<bool> *option;
	bool (AyuSettings::*get)() const;
	void (AyuSettings::*set)(bool);
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
	constexpr auto kMigratedKey = "regram.shared-settings-v1";
	auto &settings = Core::App().settings();
	if (settings.readPref<QByteArray>(kMigratedKey) != "1") {
		for (const auto &binding : kBindings) {
			if (AyuSettings::LegacySettingsLoaded()
				&& settings.readPref<QByteArray>(binding.option->key).isEmpty()) {
				Expects(Nagram::ForDevice().Set(*binding.option,
					(AyuSettings::getInstance().*binding.get)()));
			}
		}
		settings.writePref<QByteArray>(kMigratedKey, "1");
	}
	for (const auto &binding : kBindings) {
		Nagram::ForDevice().Value(*binding.option)
		| rpl::on_next([binding](bool value) {
			(AyuSettings::getInstance().*binding.set)(value);
		}, lifetime);
		AyuSettings::Changes()
		| rpl::on_next([binding] {
			Expects(Nagram::ForDevice().Set(*binding.option,
				(AyuSettings::getInstance().*binding.get)()));
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
