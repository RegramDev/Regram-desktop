#include "regram/settings/ios_backup_model.h"

#include "base/basic_types.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <array>
#include <algorithm>
#include <cmath>

namespace Regram::Settings::IosBackup {
namespace {

enum class Value { Boolean, Integer, String, Strings, Dictionary };

struct Entry {
	const char16_t *key;
	Category category;
	Value type;
	int minimum = 0;
	int maximum = 0;
};

const auto kSchema = std::array{
	Entry{ u"showTabNames", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"fontFamily", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"fontChineseFamily", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"fontImportedLatin", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"fontImportedChinese", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"fontApplyToMessages", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"fontApplyToInterface", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"accountColorsSaturation", Category::Appearance, Value::Integer, 0, 100 },
	Entry{ u"rememberLastFolder", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"hideReactions", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"disableScrollToNextChannel", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"disableScrollToNextTopic", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"disableChatSwipeOptions", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"disableDeleteChatSwipeOption", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"disableSnapDeletionEffect", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"hideTabBar", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"compactChatList", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"chatListLines", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"compactFolderNames", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"allChatsTitleLengthOverride", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"allChatsHidden", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"wideChannelPosts", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"secondsInMessages", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"hideChannelBottomButton", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"customAppBadge", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"tabBarWidthPercent", Category::Appearance, Value::Integer, 0, 100 },
	Entry{ u"hideStories", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"showRepostToStoryV2", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"nyStyle", Category::Appearance, Value::String, 0, 0 },
	Entry{ u"stickerSize", Category::Appearance, Value::Integer, 0, 200 },
	Entry{ u"stickerTimestamp", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"recentStickerLimit", Category::Appearance, Value::Integer, 1, 1000 },
	Entry{ u"native.showContactsTab", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"native.showCallsTab", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"native.foldersAtBottom", Category::Appearance, Value::Boolean, 0, 0 },
	Entry{ u"contextShowSelectFromUser", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowSaveToCloud", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowRestrict", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowHideForwardName", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowReport", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowReply", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowPin", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowSaveMedia", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowMessageReplies", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowJson", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowRepeatForward", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextShowRepeatCopy", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"contextMenuOrder", Category::Messages, Value::Strings, 0, 0 },
	Entry{ u"mentionAsUserIdLink", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"disableLinkPreview", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"defaultEmojisFirst", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"messageDoubleTapActionOutgoing", Category::Messages, Value::String, 0, 0 },
	Entry{ u"forceEmojiTab", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"inputToolbar", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"panguSpacing", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"defaultOutgoingFormatting", Category::Messages, Value::String, 0, 0 },
	Entry{ u"sendWithReturnKey", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"hideRecordingButton", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"disableSendAsButton", Category::Messages, Value::Boolean, 0, 0 },
	Entry{ u"chatPreferences", Category::Messages, Value::String, 0, 0 },
	Entry{ u"hidePhoneInSettings", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"profileDefaultTabGroupsInCommon", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"showDC", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"showCreationDate", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"showRegDate", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"showProfileId", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"confirmCalls", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"messageFilterRules", Category::Privacy, Value::String, 0, 0 },
	Entry{ u"blockedPeerIds", Category::Privacy, Value::Strings, 0, 0 },
	Entry{ u"messageFilterDisabledPeerIds", Category::Privacy, Value::Strings, 0, 0 },
	Entry{ u"antiRevokePeerIds", Category::Privacy, Value::Strings, 0, 0 },
	Entry{ u"disableAllAds", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"antiRevoke", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"antiAutoDelete", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"antiSelfDestruct", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"antiScreenshotNotification", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"allowSavingProtectedContent", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"allowDownloadingStories", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"ghostDontReadStories", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"ghostDontSendOnline", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"ghostDontSendTyping", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"storyStealthMode", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"disableSwipeToRecordStory", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"warnOnStoriesOpen", Category::Privacy, Value::Boolean, 0, 0 },
	Entry{ u"quickTranslateButton", Category::Translation, Value::Boolean, 0, 0 },
	Entry{ u"translationBackend", Category::Translation, Value::String, 0, 0 },
	Entry{ u"transcriptionBackend", Category::Translation, Value::String, 0, 0 },
	Entry{ u"outgoingLanguageTranslation", Category::Translation, Value::Dictionary, 0, 0 },
	Entry{ u"startTelescopeWithRearCam", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"forceBuiltInMic", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"uploadSpeedBoost", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"downloadSpeedBoost", Category::Media, Value::String, 0, 0 },
	Entry{ u"mediaLoadingExperiment", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"defaultVideoQuality", Category::Media, Value::String, 0, 0 },
	Entry{ u"localDNSForProxyHost", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"sendLargePhotos", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"outgoingPhotoQuality", Category::Media, Value::Integer, 0, 100 },
	Entry{ u"disableGalleryCamera", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"disableGalleryCameraPreview", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"forceSystemSharing", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"native.enableVoipTcp", Category::Media, Value::Boolean, 0, 0 },
	Entry{ u"pinnedMessageNotifications", Category::Notifications, Value::String, 0, 0 },
	Entry{ u"mentionsAndRepliesNotifications", Category::Notifications, Value::String, 0, 0 },
	Entry{ u"pinnedMessageNotificationsExceptions", Category::Notifications, Value::Dictionary, 0, 0 },
	Entry{ u"mentionsAndRepliesNotificationsExceptions", Category::Notifications, Value::Dictionary, 0, 0 },
	Entry{ u"filterNotifications", Category::Notifications, Value::Boolean, 0, 0 },
};

const Entry *Find(const QString &key) {
	for (const auto &entry : kSchema) {
		if (key == QString::fromUtf16(entry.key)) {
			return &entry;
		}
	}
	return nullptr;
}

bool ValidRules(const QString &text) {
	const auto document = QJsonDocument::fromJson(text.toUtf8());
	if (!document.isArray()) {
		return false;
	}
	for (const auto &entry : document.array()) {
		const auto rule = entry.toObject();
		if (!entry.isObject() || !rule.value(u"id"_q).isString()
			|| !rule.value(u"pattern"_q).isString()
			|| !rule.value(u"isRegex"_q).isBool()
			|| !rule.value(u"peerIds"_q).isArray()) {
			return false;
		}
		for (const auto &key : { u"isEnabled"_q, u"isException"_q }) {
			const auto value = rule.value(key);
			if (!value.isUndefined() && !value.isNull() && !value.isBool()) {
				return false;
			}
		}
		for (const auto &peer : rule.value(u"peerIds"_q).toArray()) {
			if (!peer.isDouble() || peer.toDouble() != double(peer.toInteger())) {
				return false;
			}
		}
	}
	return true;
}

bool Valid(const Entry &entry, const QJsonValue &value) {
	if (entry.type == Value::Boolean) {
		return value.isBool();
	} else if (entry.type == Value::Integer) {
		const auto number = value.toDouble();
		return value.isDouble() && std::isfinite(number) && std::floor(number) == number
			&& number >= entry.minimum && number <= entry.maximum
			&& (QString::fromUtf16(entry.key) != u"tabBarWidthPercent"_q
				|| number == 0 || number >= 50);
	} else if (entry.type == Value::String) {
		if (!value.isString()) {
			return false;
		}
		const auto text = value.toString();
		const auto key = QString::fromUtf16(entry.key);
		if (key == u"fontImportedLatin"_q || key == u"fontImportedChinese"_q) {
			if (text.isEmpty()) {
				return true;
			}
			const auto parts = text.split(u':');
			if (parts.size() > 2 || parts.front().size() != 64
				|| !std::ranges::all_of(parts.front(), [](QChar ch) {
					return (ch >= u'0' && ch <= u'9') || (ch >= u'a' && ch <= u'f')
						|| (ch >= u'A' && ch <= u'F');
				})) {
				return false;
			} else if (parts.size() == 2) {
				auto ok = false;
				const auto face = parts.back().toLongLong(&ok);
				return ok && face > 0 && QString::number(face) == parts.back();
			}
			return true;
		} else if (key == u"messageFilterRules"_q) {
			return ValidRules(text);
		} else if (key == u"chatPreferences"_q) {
			const auto document = QJsonDocument::fromJson(text.toUtf8());
			if (!document.isObject()) {
				return false;
			}
			const auto chats = document.object();
			for (auto i = chats.begin(); i != chats.end(); ++i) {
				const auto prefs = i.value().toObject();
				const auto format = prefs.value(u"formatting"_q);
				const auto spacing = prefs.value(u"panguSpacing"_q);
				if (!i.value().isObject()
					|| (!format.isUndefined() && !format.isNull() && !format.isString())
					|| (!spacing.isUndefined() && !spacing.isNull() && !spacing.isBool())) {
					return false;
				}
			}
			return true;
		}
		return text.toUtf8().size() <= 256;
	} else if (entry.type == Value::Strings) {
		if (!value.isArray() || value.toArray().size() > 10000) {
			return false;
		}
		for (const auto &item : value.toArray()) {
			if (!item.isString() || item.toString().toUtf8().size() > 128) {
				return false;
			}
		}
		return true;
	}
	if (!value.isObject() || value.toObject().size() > 10000) {
		return false;
	}
	const auto values = value.toObject();
	for (auto i = values.begin(); i != values.end(); ++i) {
		if (i.key().toUtf8().size() > 128 || !i.value().isString()
			|| i.value().toString().toUtf8().size() > 128) {
			return false;
		}
	}
	return true;
}

} // namespace

std::optional<Category> CategoryFor(const QString &key) {
	const auto entry = Find(key);
	return entry ? std::optional(entry->category) : std::nullopt;
}

Document Parse(const QByteArray &bytes) {
	if (bytes.size() > kMaximumBytes) {
		return { .error = u"The settings backup exceeds 2 MB."_q };
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(bytes, &error);
	const auto object = document.object();
	const auto date = object.value(u"createdAt"_q).toString();
	if (error.error != QJsonParseError::NoError || !document.isObject()
		|| object.value(u"version"_q) != QJsonValue(1)
		|| !object.value(u"values"_q).isObject()
		|| !date.contains(u'T')
		|| !QDateTime::fromString(date, Qt::ISODate).isValid()) {
		return { .error = u"Invalid or unsupported Regram iOS settings backup."_q };
	}
	auto result = Document{ .createdAt = date };
	const auto values = object.value(u"values"_q).toObject();
	for (auto i = values.begin(); i != values.end(); ++i) {
		const auto entry = Find(i.key());
		if (!entry) {
			continue;
		} else if (!Valid(*entry, i.value())) {
			return { .error = u"Invalid setting: "_q + i.key() };
		}
		result.values.insert(i.key(), i.value());
	}
	return result;
}

QByteArray Encode(const QJsonObject &values) {
	const auto data = QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"createdAt"_q, QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
		{ u"values"_q, Select(values, kAllCategories) },
	}).toJson(QJsonDocument::Indented);
	return Parse(data).error.isEmpty() ? data : QByteArray();
}

QJsonObject Select(const QJsonObject &values, const std::set<Category> &categories) {
	auto result = QJsonObject();
	for (auto i = values.begin(); i != values.end(); ++i) {
		const auto category = CategoryFor(i.key());
		if (category && categories.contains(*category)) {
			result.insert(i.key(), i.value());
		}
	}
	return result;
}

bool ValidateSource(const QByteArray &bytes) {
	return bytes.isEmpty() || Parse(bytes).error.isEmpty();
}

} // namespace Regram::Settings::IosBackup
