#include "regram/settings/advanced_model.h"

#include <QtCore/QJsonDocument>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace Regram::Settings {
namespace {

enum class Value { Boolean, Integer, Decimal, Text, Provider };

struct Entry {
	const char16_t *key = nullptr;
	Value type = Value::Boolean;
	double minimum = 0;
	double maximum = 0;
};

const auto kDevice = std::array{
	Entry{ u"useGlobalGhostMode", Value::Boolean, 0, 0 },
	Entry{ u"saveDeletedMessages", Value::Boolean, 0, 0 },
	Entry{ u"saveMessagesHistory", Value::Boolean, 0, 0 },
	Entry{ u"saveForBots", Value::Boolean, 0, 0 },
	Entry{ u"filtersEnabled", Value::Boolean, 0, 0 },
	Entry{ u"filtersEnabledInChats", Value::Boolean, 0, 0 },
	Entry{ u"hideFromBlocked", Value::Boolean, 0, 0 },
	Entry{ u"semiTransparentDeletedMessages", Value::Boolean, 0, 0 },
	Entry{ u"disableAds", Value::Boolean, 0, 0 },
	Entry{ u"disableCustomBackgrounds", Value::Boolean, 0, 0 },
	Entry{ u"hidePremiumStatuses", Value::Boolean, 0, 0 },
	Entry{ u"showOnlyAddedEmojisAndStickers", Value::Boolean, 0, 0 },
	Entry{ u"collapseSimilarChannels", Value::Boolean, 0, 0 },
	Entry{ u"hideSimilarChannels", Value::Boolean, 0, 0 },
	Entry{ u"messageBubbleRadius", Value::Integer, 0, 16 },
	Entry{ u"disableOpenLinkWarning", Value::Boolean, 0, 0 },
	Entry{ u"wideMultiplier", Value::Decimal, 0.5, 4 },
	Entry{ u"spoofWebviewAsAndroid", Value::Boolean, 0, 0 },
	Entry{ u"increaseWebviewHeight", Value::Boolean, 0, 0 },
	Entry{ u"increaseWebviewWidth", Value::Boolean, 0, 0 },
	Entry{ u"materialSwitches", Value::Boolean, 0, 0 },
	Entry{ u"removeMessageTail", Value::Boolean, 0, 0 },
	Entry{ u"disableNotificationsDelay", Value::Boolean, 0, 0 },
	Entry{ u"localPremium", Value::Boolean, 0, 0 },
	Entry{ u"showChannelReactions", Value::Boolean, 0, 0 },
	Entry{ u"showGroupReactions", Value::Boolean, 0, 0 },
	Entry{ u"showPrivateChatReactions", Value::Boolean, 0, 0 },
	Entry{ u"appIcon", Value::Text, 0, 0 },
	Entry{ u"simpleQuotesAndReplies", Value::Boolean, 0, 0 },
	Entry{ u"hideFastShare", Value::Boolean, 0, 0 },
	Entry{ u"replaceBottomInfoWithIcons", Value::Boolean, 0, 0 },
	Entry{ u"deletedMark", Value::Text, 0, 0 },
	Entry{ u"editedMark", Value::Text, 0, 0 },
	Entry{ u"unlimitedRecentStickers", Value::Boolean, 0, 0 },
	Entry{ u"showReactionsPanelInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showViewsPanelInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showHideMessageInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showUserMessagesInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showMessageDetailsInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showRepeatMessageInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showAddFilterInContextMenu", Value::Integer, 0, 2 },
	Entry{ u"showAttachButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showCommandsButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showEmojiButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showMicrophoneButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showAutoDeleteButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showGiftButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showAiEditorButtonInMessageField", Value::Boolean, 0, 0 },
	Entry{ u"showAttachPopup", Value::Boolean, 0, 0 },
	Entry{ u"showEmojiPopup", Value::Boolean, 0, 0 },
	Entry{ u"showMyProfileInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showBotsInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showNewGroupInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showNewChannelInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showContactsInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showCallsInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showSavedMessagesInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showLReadToggleInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showSReadToggleInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showNightModeToggleInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showGhostToggleInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showStreamerToggleInDrawer", Value::Boolean, 0, 0 },
	Entry{ u"showGhostToggleInTray", Value::Boolean, 0, 0 },
	Entry{ u"showStreamerToggleInTray", Value::Boolean, 0, 0 },
	Entry{ u"monoFont", Value::Text, 0, 0 },
	Entry{ u"hideNotificationCounters", Value::Boolean, 0, 0 },
	Entry{ u"channelBottomButton", Value::Integer, 0, 2 },
	Entry{ u"quickAdminShortcuts", Value::Boolean, 0, 0 },
	Entry{ u"disableGreetingSticker", Value::Boolean, 0, 0 },
	Entry{ u"showPeerId", Value::Integer, 0, 2 },
	Entry{ u"showMessageShot", Value::Boolean, 0, 0 },
	Entry{ u"filterZalgo", Value::Boolean, 0, 0 },
	Entry{ u"voiceConfirmation", Value::Boolean, 0, 0 },
	Entry{ u"roundConfirmation", Value::Boolean, 0, 0 },
	Entry{ u"translationProvider", Value::Provider, 0, 0 },
	Entry{ u"adaptiveCoverColor", Value::Boolean, 0, 0 },
	Entry{ u"improveLinkPreviews", Value::Boolean, 0, 0 },
	Entry{ u"crashReporting", Value::Boolean, 0, 0 },
	Entry{ u"avatarCorners", Value::Integer, 0, 23 },
	Entry{ u"singleCornerRadius", Value::Boolean, 0, 0 },
	Entry{ u"streamerMode", Value::Boolean, 0, 0 },
};

const auto kGhost = std::array{
	Entry{ u"sendReadMessages", Value::Boolean },
	Entry{ u"sendReadStories", Value::Boolean },
	Entry{ u"sendOnlinePackets", Value::Boolean },
	Entry{ u"sendUploadProgress", Value::Boolean },
	Entry{ u"sendOfflinePacketAfterOnline", Value::Boolean },
	Entry{ u"markReadAfterAction", Value::Boolean },
	Entry{ u"useScheduledMessages", Value::Boolean },
	Entry{ u"sendWithoutSound", Value::Integer, 0, 2 },
	Entry{ u"suggestGhostModeBeforeViewingStory", Value::Boolean },
	Entry{ u"sendReadMessagesLocked", Value::Boolean },
	Entry{ u"sendReadStoriesLocked", Value::Boolean },
	Entry{ u"sendOnlinePacketsLocked", Value::Boolean },
	Entry{ u"sendUploadProgressLocked", Value::Boolean },
	Entry{ u"sendOfflinePacketAfterOnlineLocked", Value::Boolean },
};

const auto kShot = std::array{
	Entry{ u"showBackground", Value::Boolean },
	Entry{ u"showDate", Value::Boolean },
	Entry{ u"showReactions", Value::Boolean },
	Entry{ u"showHeaderDecorations", Value::Boolean },
	Entry{ u"showColorfulReplies", Value::Boolean },
	Entry{ u"revealSpoilers", Value::Boolean },
	Entry{ u"embeddedThemeType", Value::Integer, -1, 3 },
	Entry{ u"embeddedThemeAccentColor", Value::Integer, 0, 4294967295.0 },
};

QJsonObject Pick(const QJsonObject &source, std::span<const Entry> schema) {
	auto result = QJsonObject();
	for (const auto &entry : schema) {
		const auto key = QString::fromUtf16(entry.key);
		if (source.contains(key)) {
			result.insert(key, source.value(key));
		}
	}
	return result;
}

bool ValidValues(const QJsonObject &values, std::span<const Entry> schema) {
	for (auto i = values.begin(); i != values.end(); ++i) {
		const auto found = std::find_if(schema.begin(), schema.end(), [&](const auto &entry) {
			return QString::fromUtf16(entry.key) == i.key();
		});
		if (found == schema.end()) {
			return false;
		}
		const auto value = i.value();
		if (found->type == Value::Boolean) {
			if (!value.isBool()) {
				return false;
			}
		} else if (found->type == Value::Provider) {
			const auto providers = std::array{ u"telegram", u"google", u"yandex", u"native" };
			if (!value.isString() || !std::any_of(providers.begin(), providers.end(), [&](const auto provider) {
				return value.toString() == QString::fromUtf16(provider);
			})) {
				return false;
			}
		} else if (found->type == Value::Text) {
			const auto text = value.toString();
			if (!value.isString() || text.size() > 256 || text.contains(QChar(0))
				|| text.contains(u'\n') || text.contains(u'\r')
				|| QString::fromUtf8(text.toUtf8()) != text) {
				return false;
			}
		} else {
			const auto number = value.toDouble();
			if (!value.isDouble() || !std::isfinite(number)
				|| number < found->minimum || number > found->maximum
				|| (found->type == Value::Integer && std::floor(number) != number)) {
				return false;
			}
		}
	}
	return true;
}

} // namespace

QByteArray PortableAdvancedSettings(const QJsonObject &source) {
	const auto result = QJsonObject{
		{ u"version"_q, 1 },
		{ u"settings"_q, Pick(source, kDevice) },
		{ u"globalGhost"_q, Pick(source.value(u"ghostModeSettings"_q)
			.toObject().value(u"0"_q).toObject(), kGhost) },
		{ u"messageShot"_q, Pick(source.value(u"messageShotSettings"_q).toObject(), kShot) },
	};
	return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

std::optional<QJsonObject> ParseAdvancedSettings(const QByteArray &raw) {
	if (raw.size() > 64 * 1024) {
		return std::nullopt;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	const auto object = document.object();
	if (error.error != QJsonParseError::NoError || !document.isObject()
		|| object.size() != 4 || object.value(u"version"_q) != QJsonValue(1)) {
		return std::nullopt;
	}
	for (const auto &[key, schema] : {
			std::pair{ u"settings"_q, std::span<const Entry>(kDevice) },
			std::pair{ u"globalGhost"_q, std::span<const Entry>(kGhost) },
			std::pair{ u"messageShot"_q, std::span<const Entry>(kShot) } }) {
		const auto value = object.value(key);
		if (!value.isObject() || !ValidValues(value.toObject(), schema)) {
			return std::nullopt;
		}
	}
	return object;
}

bool ValidAdvancedSettings(const QByteArray &raw) {
	return raw.isEmpty() || ParseAdvancedSettings(raw).has_value();
}

} // namespace Regram::Settings
