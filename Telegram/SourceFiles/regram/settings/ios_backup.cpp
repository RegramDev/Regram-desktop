#include "regram/settings/ios_backup.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "lang_auto_counts.h"
#include "platform/platform_translate_provider.h"
#include "main/main_session.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/filters/runtime.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/core/language.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/media/options.h"
#include "regram/nagram/menu/model.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/network/options.h"
#include "regram/nagram/notifications/model.h"
#include "regram/typography/fonts.h"
#include "ui/widgets/fields/input_field.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/settings/restart.h"
#include "storage/localstorage.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>
#include <array>
#include <iterator>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Regram::Settings::IosBackup {
namespace {

struct Change {
	QString title;
	QString preview;
	Fn<bool()> current;
	Fn<void()> apply;
	bool restart = false;
};

struct Binding {
	QString key;
	Fn<QJsonValue()> read;
	Fn<std::optional<Change>(QJsonValue)> prepare;
	Fn<QJsonValue()> defaults;
};

struct Selection {
	std::set<Category> categories = kAllCategories;
	rpl::variable<bool> any = true;
};

QString Title(std::string_view key) {
	const auto encoded = QByteArray(key.data(), key.size());
	const auto index = Lang::GetKeyIndex(QLatin1String(encoded));
	return index == Lang::kKeysCount ? QString::fromUtf8(key.data(), key.size())
		: Nagram::LocalizedValue(Lang::GetInstance(), index);
}

QString ValueText(const QJsonValue &value, const QString &key) {
	if (value.isBool()) {
		if (key.startsWith(u"contextShow"_q)) {
			return value.toBool() ? tr::lng_nagram_menu_shown(tr::now)
				: tr::lng_regram_menu_submenu(tr::now);
		}
		return value.toBool() ? tr::lng_nagram_config_on(tr::now) : tr::lng_nagram_config_off(tr::now);
	} else if (value.isNull() || value.isUndefined()) {
		return u"—"_q;
	} else if (value.isArray()) {
		return tr::lng_regram_backup_items(tr::now, lt_amount, QString::number(value.toArray().size()));
	} else if (value.isDouble()) {
		return QString::number(value.toInteger());
	}
	if (key == u"messageFilterRules"_q || key == u"chatPreferences"_q) {
		const auto parsed = QJsonDocument::fromJson(value.toString().toUtf8());
		return tr::lng_regram_backup_items(tr::now, lt_amount,
			QString::number(parsed.isArray() ? parsed.array().size() : parsed.object().size()));
	}
	auto result = value.toString();
	if (result.size() > 120) {
		result.truncate(120);
		if (result.back().isHighSurrogate()) {
			result.chop(1);
		}
		result += QChar(0x2026);
	}
	return result;
}

template <typename Type>
Binding OptionBinding(
		const QString &key,
		not_null<Main::Session*> session,
		Nagram::Option<Type> option,
		Fn<QJsonValue(Type)> encode,
		Fn<std::optional<Type>(QJsonValue)> decode) {
	const auto options = option.scope == Nagram::Scope::Account
		? &Nagram::ForAccount(session) : &Nagram::ForDevice();
	return {
		.key = key,
		.read = [=] { return encode(options->Get(option)); },
		.prepare = [=](QJsonValue value) -> std::optional<Change> {
			const auto after = decode(value);
			if (!after || (option.validate && !option.validate(*after))) {
				return std::nullopt;
			}
			const auto before = options->Get(option);
			return Change{
				.title = Title(option.titleKey),
				.current = [=] { return options->Get(option) == before; },
				.apply = [=] { Expects(options->Set(option, *after)); },
				.restart = (option.flags & static_cast<unsigned>(Nagram::Flag::RequiresRestart)) != 0,
			};
		},
		.defaults = [=] { return encode(option.fallback); },
	};
}

Binding BoolBinding(
		const QString &key,
		not_null<Main::Session*> session,
		Nagram::Option<bool> option,
		bool inverted = false) {
	return OptionBinding<bool>(key, session, option,
		[=](bool value) { return QJsonValue(value != inverted); },
		[=](QJsonValue value) -> std::optional<bool> {
			return value.isBool() ? std::optional(value.toBool() != inverted) : std::nullopt;
		});
}

Binding IntBinding(
		const QString &key,
		not_null<Main::Session*> session,
		Nagram::Option<int> option,
		bool text = false) {
	return OptionBinding<int>(key, session, option,
		[=](int value) {
			if (!value && (key == u"recentStickerLimit"_q || key == u"chatListLines"_q)) {
				return QJsonValue();
			}
			return text ? QJsonValue(QString::number(value)) : QJsonValue(value);
		},
		[=](QJsonValue value) -> std::optional<int> {
			if (!text) {
				return value.isDouble() ? std::optional(value.toInt()) : std::nullopt;
			}
			auto ok = false;
			const auto number = value.toString().toInt(&ok);
			return ok && QString::number(number) == value.toString() ? std::optional(number) : std::nullopt;
		});
}

PeerAddress Address(PeerId peer) {
	return {
		.kind = peerIsUser(peer) ? PeerKind::User
			: peerIsChat(peer) ? PeerKind::Group : PeerKind::Channel,
		.id = peer.value & PeerId::kChatTypeMask,
	};
}

std::optional<PeerId> IosPeer(const QString &value) {
	auto ok = false;
	const auto number = value.toLongLong(&ok);
	const auto address = ok && QString::number(number) == value
		? FromIosPeerId(number) : std::nullopt;
	if (!address) {
		return std::nullopt;
	}
	switch (address->kind) {
	case PeerKind::User: return peerFromUser(UserId(address->id));
	case PeerKind::Group: return peerFromChat(ChatId(address->id));
	case PeerKind::Channel: return peerFromChannel(ChannelId(address->id));
	}
	return std::nullopt;
}

Binding PeersBinding(
		const QString &key,
		not_null<Main::Session*> session,
		Nagram::Option<QByteArray> option,
		bool usersOnly) {
	return OptionBinding<QByteArray>(key, session, option,
		[=](QByteArray raw) {
			auto result = QJsonArray();
			for (const auto &entry : QJsonDocument::fromJson(raw).array()) {
				const auto number = ParseServerId(entry.toString().toStdString());
				const auto ios = number ? ToIosPeerId(Address(DeserializePeerId(*number))) : std::nullopt;
				if (!ios) {
					return QJsonValue();
				}
				result.push_back(QString::number(*ios));
			}
			return QJsonValue(result);
		},
		[=](QJsonValue value) -> std::optional<QByteArray> {
			auto result = QJsonArray();
			auto seen = std::set<uint64>();
			for (const auto &entry : value.toArray()) {
				const auto peer = IosPeer(entry.toString());
				if (!peer || (usersOnly && (!peerIsUser(*peer) || *peer == session->user()->id))) {
					return std::nullopt;
				}
				const auto id = SerializePeerId(*peer);
				if (seen.insert(id).second) {
					result.push_back(QString::number(id));
				}
			}
			return QJsonDocument(result).toJson(QJsonDocument::Compact);
		});
}

const auto kMenuItems = std::array{
	std::pair{ u"repeatForward", Nagram::Menu::ActionId::Repeat },
	std::pair{ u"repeatCopy", Nagram::Menu::ActionId::RepeatAsCopy },
	std::pair{ u"forwardNoQuote", Nagram::Menu::ActionId::ForwardWithoutQuote },
	std::pair{ u"saveToCloud", Nagram::Menu::ActionId::SaveToSaved },
	std::pair{ u"reply", Nagram::Menu::ActionId::Reply },
	std::pair{ u"selectFromUser", Nagram::Menu::ActionId::SelectSender },
	std::pair{ u"saveMedia", Nagram::Menu::ActionId::Image },
	std::pair{ u"messageReplies", Nagram::Menu::ActionId::Replies },
	std::pair{ u"pin", Nagram::Menu::ActionId::Pin },
	std::pair{ u"restrict", Nagram::Menu::ActionId::RestrictSender },
	std::pair{ u"report", Nagram::Menu::ActionId::Report },
	std::pair{ u"json", Nagram::Menu::ActionId::MessageJson },
};

Binding MenuBinding(const QString &key, Nagram::Menu::ActionId id) {
	using namespace Nagram;
	return {
		.key = key,
		.read = [=] {
			if (Menu::ReadVisibility(ForDevice().Get(Menu::kMenuConfig), id) != Menu::Visibility::Show) {
				return QJsonValue();
			}
			return QJsonValue(!ranges::contains(Menu::ReadActionList(ForDevice().Get(Menu::kRegramSubmenu)), id));
		},
		.prepare = [=](QJsonValue value) -> std::optional<Change> {
			const auto before = ForDevice().Get(Menu::kMenuConfig);
			const auto groups = ForDevice().Get(Menu::kRegramSubmenu);
			const auto entry = ranges::find(Menu::kEntries, id, &Menu::Entry::id);
			return Change{
				.title = Title(entry->titleKey),
				.current = [=] { return ForDevice().Get(Menu::kMenuConfig) == before
					&& ForDevice().Get(Menu::kRegramSubmenu) == groups; },
				.apply = [=] {
					auto grouped = Menu::ReadActionList(ForDevice().Get(Menu::kRegramSubmenu));
					std::erase(grouped, id);
					if (!value.toBool()) {
						grouped.push_back(id);
					}
					Expects(ForDevice().Set(Menu::kRegramSubmenu, Menu::WriteActionList(grouped)));
					Expects(ForDevice().Set(Menu::kMenuConfig, Menu::WriteVisibility(
						ForDevice().Get(Menu::kMenuConfig), id, Menu::Visibility::Show)));
				},
			};
		},
		.defaults = [=] { return QJsonValue(true); },
	};
}

const auto kFormattingNames = std::array{
	u"none", u"bold", u"italic", u"underline", u"strikethrough", u"spoiler",
};

std::optional<int> Formatting(const QString &value) {
	for (auto i = 0; i != int(kFormattingNames.size()); ++i) {
		if (value == QString::fromUtf16(kFormattingNames[i])) {
			return i;
		}
	}
	return std::nullopt;
}

Binding ChatPreferencesBinding(not_null<Main::Session*> session) {
	using namespace Nagram;
	return OptionBinding<QByteArray>(u"chatPreferences"_q, session, Compose::kChatPreferences,
		[=](QByteArray raw) {
			const auto parsed = Compose::ParseChatPreferences(raw);
			if (!parsed || (!raw.isEmpty() && parsed->user != session->userId().bare)) {
				return QJsonValue();
			}
			const auto account = ToIosPeerId(Address(session->user()->id));
			auto result = QJsonObject();
			if (!account) {
				return QJsonValue();
			}
			for (const auto &[peer, prefs] : parsed->chats) {
				const auto ios = ToIosPeerId(Address(PeerId(peer)));
				if (!ios) {
					return QJsonValue();
				}
				auto value = QJsonObject();
				if (prefs.formatting >= 0) {
					value.insert(u"formatting"_q, QString::fromUtf16(kFormattingNames[prefs.formatting]));
				}
				if (prefs.spacing >= 0) {
					value.insert(u"panguSpacing"_q, prefs.spacing != 0);
				}
				result.insert(QString::number(*account) + u':' + QString::number(*ios), value);
			}
			return QJsonValue(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
		},
		[=](QJsonValue value) -> std::optional<QByteArray> {
			const auto object = QJsonDocument::fromJson(value.toString().toUtf8()).object();
			auto result = Compose::ChatPreferenceSet{ .user = session->userId().bare };
			for (auto i = object.begin(); i != object.end(); ++i) {
				const auto parts = i.key().split(u':');
				if (parts.size() != 2) {
					return std::nullopt;
				}
				const auto account = IosPeer(parts.front());
				const auto peer = IosPeer(parts.back());
				if (!account || !peer) {
					return std::nullopt;
				} else if (*account != session->user()->id) {
					continue;
				}
				const auto prefs = i.value().toObject();
				const auto format = prefs.value(u"formatting"_q);
				const auto number = format.isUndefined() || format.isNull()
					? std::optional(-1) : Formatting(format.toString());
				if (!number) {
					return std::nullopt;
				}
				const auto space = prefs.value(u"panguSpacing"_q);
				result.chats.emplace(peer->value, Compose::ChatPreferences{
					.formatting = *number,
					.spacing = space.isUndefined() || space.isNull() ? -1 : space.toBool() ? 1 : 0,
				});
			}
			return Compose::EncodeChatPreferences(result);
		});
}

Binding TranslationBinding() {
	using namespace Nagram;
	return {
		.key = u"translationBackend"_q,
		.read = [] {
			const auto config = Services();
			if (!config) {
				return QJsonValue();
			}
			const auto selected = config->value(u"translation"_q).toString();
			if (selected == u"system"_q) {
				return QJsonValue(u"system"_q);
			} else if (selected == u"telegram"_q) {
				return QJsonValue(u"default"_q);
			} else if (!selected.isEmpty()) {
				return QJsonValue();
			}
			switch (AyuSettings::getInstance().translationProvider()) {
			case TranslationProvider::Telegram: return QJsonValue(u"default"_q);
			case TranslationProvider::Google: return QJsonValue(u"gtranslate"_q);
			case TranslationProvider::Native: return QJsonValue(u"system"_q);
			}
			return QJsonValue();
		},
		.prepare = [](QJsonValue value) -> std::optional<Change> {
			const auto config = Services();
			const auto name = value.toString();
			if (!config || (name != u"default"_q && name != u"google"_q
				&& name != u"gtranslate"_q && name != u"system"_q)
				|| (name == u"system"_q && !Platform::IsTranslateProviderAvailable())) {
				return std::nullopt;
			}
			const auto raw = ForDevice().Get(kServicesConfig);
			const auto before = AyuSettings::getInstance().translationProvider();
			return Change{
				.title = tr::lng_nagram_service_translation(tr::now),
				.current = [=] { return ForDevice().Get(kServicesConfig) == raw
					&& AyuSettings::getInstance().translationProvider() == before; },
				.apply = [=] {
					auto updated = *config;
					updated.insert(u"translation"_q, QString());
					Expects(SetServices(updated));
					AyuSettings::getInstance().setTranslationProvider(name == u"system"_q
						? TranslationProvider::Native : name == u"default"_q
						? TranslationProvider::Telegram : TranslationProvider::Google);
				},
			};
		},
		.defaults = [=] { return QJsonValue(u"default"_q); },
	};
}

Binding NativeBoolean(
		const QString &key,
		QString title,
		Fn<bool()> read,
		Fn<void(bool)> apply) {
	return {
		.key = key,
		.read = [=] { return QJsonValue(read()); },
		.prepare = [=](QJsonValue value) -> std::optional<Change> {
			if (!value.isBool()) {
				return std::nullopt;
			}
			const auto before = read();
			const auto global = AyuSettings::getInstance().useGlobalGhostMode();
			return Change{
				.title = title,
				.current = [=] { return read() == before
					&& (!key.startsWith(u"ghost"_q)
						|| AyuSettings::getInstance().useGlobalGhostMode() == global); },
				.apply = [=] { apply(value.toBool()); },
			};
		},
		.defaults = [=] { return QJsonValue(false); },
	};
}

Binding ChannelBottomBinding() {
	using namespace Nagram;
	return {
		.key = u"hideChannelBottomButton"_q,
		.read = [] { return QJsonValue(AyuSettings::getInstance().channelBottomButton()
			== ChannelBottomButton::Hidden); },
		.prepare = [](QJsonValue value) -> std::optional<Change> {
			const auto before = AyuSettings::getInstance().channelBottomButton();
			const auto hidden = ForDevice().Get(Compose::kHideChannelMuteButton);
			return Change{
				.title = Title(Compose::kHideChannelMuteButton.titleKey),
				.current = [=] { return AyuSettings::getInstance().channelBottomButton() == before
					&& ForDevice().Get(Compose::kHideChannelMuteButton) == hidden; },
				.apply = [=] {
					AyuSettings::getInstance().setChannelBottomButton(value.toBool()
						? ChannelBottomButton::Hidden : before == ChannelBottomButton::Hidden
						? ChannelBottomButton::MuteUnmute : before);
					Expects(ForDevice().Set(Compose::kHideChannelMuteButton, false));
				},
			};
		},
		.defaults = [=] { return QJsonValue(false); },
	};
}

Binding NotificationExceptionsBinding(
		const QString &key,
		not_null<Main::Session*> session,
		Nagram::Option<QByteArray> option) {
	return OptionBinding<QByteArray>(key, session, option,
		[](QByteArray bytes) {
			const auto stored = QJsonDocument::fromJson(bytes).object();
			auto result = QJsonObject();
			for (auto i = stored.begin(); i != stored.end(); ++i) {
				const auto peer = DeserializePeerId(i.key().toULongLong());
				if (!peerIsChat(peer)) {
					return QJsonValue();
				}
				result.insert(QString::number(peerToChat(peer).bare), i.value());
			}
			return QJsonValue(result);
		},
		[](QJsonValue value) -> std::optional<QByteArray> {
			auto result = QJsonObject();
			const auto values = value.toObject();
			for (auto i = values.begin(); i != values.end(); ++i) {
				auto valid = false;
				const auto id = i.key().toULongLong(&valid);
				if (!valid || !id || id > PeerId::kChatTypeMask || QString::number(id) != i.key()
					|| !i.value().isString() || !Nagram::Notifications::ValidPolicy(i.value().toString())) {
					return std::nullopt;
				}
				result.insert(QString::number(SerializePeerId(peerFromChat(ChatId(id)))), i.value());
			}
			return QJsonDocument(result).toJson(QJsonDocument::Compact);
		});
}

std::vector<Binding> Bindings(not_null<Main::Session*> session, const QJsonObject &selected = {}) {
	using namespace Nagram;
	auto result = std::vector<Binding>();
	const auto fontKeys = QStringList{ u"fontFamily"_q, u"fontChineseFamily"_q,
		u"fontImportedLatin"_q, u"fontImportedChinese"_q, u"fontApplyToMessages"_q, u"fontApplyToInterface"_q };
	auto desiredFonts = Typography::IosFontSettings();
	for (const auto &key : fontKeys) {
		if (selected.contains(key)) {
			desiredFonts.insert(key, selected.value(key));
		}
	}
	const auto resolvedFonts = Typography::ResolveIosFonts(desiredFonts);
	for (const auto &key : fontKeys) {
		result.push_back(Binding{
			.key = key,
			.read = [=] { return Typography::IosFontSettings().value(key); },
			.prepare = [=](QJsonValue value) -> std::optional<Change> {
				if (!resolvedFonts || value != desiredFonts.value(key)) {
					return std::nullopt;
				}
				const auto before = Typography::NativeFontSettings();
				return Change{
					.title = tr::lng_regram_fonts(tr::now),
					.current = [=] { return Typography::NativeFontSettings() == before; },
					.apply = [=] { Typography::ApplyIosFonts(*resolvedFonts); },
					.restart = true,
				};
			},
			.defaults = [=] { return key.startsWith(u"fontApply"_q) ? QJsonValue(true)
				: QJsonValue(key.startsWith(u"fontImported"_q) ? QString() : u"system"_q); },
		});
	}
	result.push_back(BoolBinding(u"antiAutoDelete"_q, session, Privacy::kAntiAutoDelete));
	result.push_back(BoolBinding(u"antiSelfDestruct"_q, session, Privacy::kAntiSelfDestruct));
	result.push_back(PeersBinding(u"antiRevokePeerIds"_q, session, Privacy::kAntiRevokePeers, false));
	result.push_back(OptionBinding<QString>(u"pinnedMessageNotifications"_q, session, Notifications::kPinnedPolicy,
		[](QString value) { return QJsonValue(value); },
		[](QJsonValue value) -> std::optional<QString> { return value.isString() && Notifications::ValidPolicy(value.toString())
			? std::optional(value.toString()) : std::nullopt; }));
	result.push_back(OptionBinding<QString>(u"mentionsAndRepliesNotifications"_q, session, Notifications::kMentionPolicy,
		[](QString value) { return QJsonValue(value); },
		[](QJsonValue value) -> std::optional<QString> { return value.isString() && Notifications::ValidPolicy(value.toString())
			? std::optional(value.toString()) : std::nullopt; }));
	result.push_back(NotificationExceptionsBinding(u"pinnedMessageNotificationsExceptions"_q, session, Notifications::kPinnedExceptions));
	result.push_back(NotificationExceptionsBinding(u"mentionsAndRepliesNotificationsExceptions"_q, session, Notifications::kMentionExceptions));
	result.push_back(BoolBinding(u"mediaLoadingExperiment"_q, session, Nagram::Media::kLoadingExperiment));
	result.push_back(OptionBinding<int>(u"defaultVideoQuality"_q, session, Nagram::Media::kDefaultVideoQuality,
		[](int value) { return QJsonValue(value == 1 ? u"highest"_q : value == 2 ? u"lowest"_q : u"automatic"_q); },
		[](QJsonValue value) -> std::optional<int> {
			return value == u"automatic"_q ? std::optional(0) : value == u"highest"_q ? std::optional(1)
				: value == u"lowest"_q ? std::optional(2) : std::nullopt;
		}));
	result.push_back(NativeBoolean(u"sendWithReturnKey"_q, tr::lng_nagram_compose(tr::now),
		[] { return Core::App().settings().sendSubmitWay() == Ui::InputSubmitSettings::Enter; },
		[](bool value) { Core::App().settings().setSendSubmitWay(value ? Ui::InputSubmitSettings::Enter : Ui::InputSubmitSettings::CtrlEnter); }));
	result.push_back(NativeBoolean(u"sendLargePhotos"_q, tr::lng_nagram_media(tr::now),
		[] { return Core::App().settings().sendFilesWay().sendLargePhotos(); },
		[](bool value) {
			auto way = Core::App().settings().sendFilesWay();
			way.setSendLargePhotos(value);
			Core::App().settings().setSendFilesWay(way);
		}));
	result.push_back(BoolBinding(u"wideChannelPosts"_q, session, Interface::kWideChannelPosts));
	result.push_back(BoolBinding(u"secondsInMessages"_q, session, Messages::kSecondsInMessages));
	result.push_back(BoolBinding(u"hideReactions"_q, session, Messages::kHideReactions));
	result.push_back(BoolBinding(u"compactChatList"_q, session, Chats::kCompactList));
	result.push_back(BoolBinding(u"compactFolderNames"_q, session, Chats::kCompactFolderTabs));
	result.push_back(BoolBinding(u"allChatsHidden"_q, session, Chats::kHideAllChatsFolder));
	result.push_back(BoolBinding(u"hideStories"_q, session, Chats::kHideStories));
	result.push_back(BoolBinding(u"disableScrollToNextChannel"_q, session, Chats::kDisableScrollToNextChannel));
	result.push_back(BoolBinding(u"disableScrollToNextTopic"_q, session, Chats::kDisableScrollToNextTopic));
	result.push_back(BoolBinding(u"disableAllAds"_q, session, Chats::kHideSponsoredMessages));
	result.push_back(BoolBinding(u"mentionAsUserIdLink"_q, session, Compose::kNicknameMentions));
	result.push_back(BoolBinding(u"disableLinkPreview"_q, session, Compose::kDisableLinkPreview));
	result.push_back(BoolBinding(u"inputToolbar"_q, session, Compose::kFormatToolbar));
	result.push_back(BoolBinding(u"panguSpacing"_q, session, Compose::kSpaceOnSend));
	result.push_back(BoolBinding(u"hideRecordingButton"_q, session, Compose::kHideRecordingButton));
	result.push_back(BoolBinding(u"disableSendAsButton"_q, session, Compose::kHideSendAsButton));
	result.push_back(ChannelBottomBinding());
	result.push_back(BoolBinding(u"confirmCalls"_q, session, Compose::kConfirmPrivateCall));
	result.push_back(BoolBinding(u"profileDefaultTabGroupsInCommon"_q, session, Privacy::kProfileCommonGroupsFirst));
	result.push_back(BoolBinding(u"showDC"_q, session, Privacy::kShowProfileDc));
	result.push_back(BoolBinding(u"showRegDate"_q, session, Privacy::kShowRegistrationDate));
	result.push_back(BoolBinding(u"allowSavingProtectedContent"_q, session, Privacy::kForceCopy));
	result.push_back(BoolBinding(u"quickTranslateButton"_q, session, kQuickTranslateButton));
	result.push_back(BoolBinding(u"uploadSpeedBoost"_q, session, Network::kUploadSpeedBoost));
	result.push_back(BoolBinding(u"localDNSForProxyHost"_q, session, Network::kUseSystemDns));
	result.push_back(BoolBinding(u"filterNotifications"_q, session, Regram::Filters::kSuppressNotifications));
	result.push_back(BoolBinding(u"stickerTimestamp"_q, session, Nagram::Media::kHideStickerTime, true));
	result.push_back(IntBinding(u"chatListLines"_q, session, Chats::kPreviewLines, true));
	result.push_back(IntBinding(u"stickerSize"_q, session, Nagram::Media::kStickerScale));
	result.push_back(IntBinding(u"recentStickerLimit"_q, session, Nagram::Media::kRecentStickerLimit));
	result.push_back(PeersBinding(u"blockedPeerIds"_q, session, Regram::Filters::kHiddenSenders, true));
	result.push_back(PeersBinding(u"messageFilterDisabledPeerIds"_q, session, Regram::Filters::kDisabledChats, false));
	result.push_back(OptionBinding<QByteArray>(u"messageFilterRules"_q, session, Regram::Filters::kPortableRules,
		[](QByteArray bytes) {
			const auto parsed = Regram::Filters::ImportStoredRules(bytes);
			const auto exported = parsed.error.isEmpty() ? Regram::Filters::ExportIosRules(parsed.rules)
				: Regram::Filters::ExportResult{ .error = parsed.error };
			return exported.error.isEmpty() ? QJsonValue(QString::fromUtf8(exported.data)) : QJsonValue();
		},
		[](QJsonValue value) -> std::optional<QByteArray> {
			const auto parsed = Regram::Filters::ImportRules(value.toString().toUtf8());
			return parsed.error.isEmpty() ? std::optional(Regram::Filters::ExportStoredRules(parsed.rules)) : std::nullopt;
		}));
	result.push_back(OptionBinding<QString>(u"downloadSpeedBoost"_q, session, Network::kDownloadSpeedBoost,
		[](QString value) { return QJsonValue(value == u"none"_q ? u"none"_q : u"medium"_q); },
		[](QJsonValue value) -> std::optional<QString> {
			return value == u"none"_q ? std::optional(u"none"_q)
				: value == u"medium"_q ? std::optional(u"balanced"_q) : std::nullopt;
		}));
	result.push_back(OptionBinding<int>(u"defaultOutgoingFormatting"_q, session, Compose::kDefaultFormatting,
		[](int value) {
			const auto names = std::array{ u"none", u"bold", u"italic", u"underline", u"strikethrough", u"spoiler" };
			return QJsonValue(QString::fromUtf16(names[value]));
		},
		[](QJsonValue value) -> std::optional<int> {
			const auto names = std::array{ u"none", u"bold", u"italic", u"underline", u"strikethrough", u"spoiler" };
			for (auto i = 0; i != int(names.size()); ++i) {
				if (value == QString::fromUtf16(names[i])) {
					return i;
				}
			}
			return std::nullopt;
		}));
	result.push_back(MenuBinding(u"contextShowRepeatForward"_q, Menu::ActionId::Repeat));
	result.push_back(MenuBinding(u"contextShowRepeatCopy"_q, Menu::ActionId::RepeatAsCopy));
	result.push_back(MenuBinding(u"contextShowHideForwardName"_q, Menu::ActionId::ForwardWithoutQuote));
	result.push_back(MenuBinding(u"contextShowSaveToCloud"_q, Menu::ActionId::SaveToSaved));
	result.push_back(MenuBinding(u"contextShowReply"_q, Menu::ActionId::Reply));
	result.push_back(MenuBinding(u"contextShowSelectFromUser"_q, Menu::ActionId::SelectSender));
	result.push_back(MenuBinding(u"contextShowSaveMedia"_q, Menu::ActionId::Image));
	result.push_back(MenuBinding(u"contextShowMessageReplies"_q, Menu::ActionId::Replies));
	result.push_back(MenuBinding(u"contextShowPin"_q, Menu::ActionId::Pin));
	result.push_back(MenuBinding(u"contextShowRestrict"_q, Menu::ActionId::RestrictSender));
	result.push_back(MenuBinding(u"contextShowReport"_q, Menu::ActionId::Report));
	result.push_back(MenuBinding(u"contextShowJson"_q, Menu::ActionId::MessageJson));
	result.push_back(OptionBinding<QByteArray>(u"contextMenuOrder"_q, session, Menu::kMenuOrder,
		[](QByteArray bytes) {
			auto values = QJsonArray();
			for (const auto id : Menu::ReadActionOrder(bytes)) {
				for (const auto &[name, mapped] : kMenuItems) {
					if (id == mapped) {
						values.push_back(QString::fromUtf16(name));
					}
				}
			}
			return QJsonValue(values);
		},
		[](QJsonValue value) -> std::optional<QByteArray> {
			auto ids = std::vector<Menu::ActionId>();
			for (const auto &entry : value.toArray()) {
				const auto found = ranges::find_if(kMenuItems, [&](const auto &mapping) {
					return entry == QString::fromUtf16(mapping.first);
				});
				if (found != kMenuItems.end() && !ranges::contains(ids, found->second)) {
					ids.push_back(found->second);
				}
			}
			return Menu::WriteActionList(ids);
		}));
	result.push_back(ChatPreferencesBinding(session));
	result.push_back(TranslationBinding());
	result.push_back(OptionBinding<int>(u"showProfileId"_q, session, Privacy::kProfileIdFormat,
		[](int value) { return QJsonValue(value != 0); },
		[](QJsonValue value) -> std::optional<int> {
			const auto current = ForDevice().Get(Privacy::kProfileIdFormat);
			return value.toBool() ? (current ? current : 2) : 0;
		}));
	result.push_back(NativeBoolean(u"antiRevoke"_q, tr::lng_regram_backup_anti_revoke(tr::now),
		[] { return AyuSettings::getInstance().saveDeletedMessages(); },
		[](bool value) { AyuSettings::getInstance().setSaveDeletedMessages(value); }));
	result.push_back(NativeBoolean(u"ghostDontReadStories"_q, tr::ayu_DontReadStories(tr::now),
		[=] { return !AyuSettings::ghost(session).sendReadStories(); },
		[=](bool value) { AyuSettings::ghost(session).setSendReadStories(!value); }));
	result.push_back(NativeBoolean(u"ghostDontSendOnline"_q, tr::ayu_DontSendOnlinePackets(tr::now),
		[=] { return !AyuSettings::ghost(session).sendOnlinePackets(); },
		[=](bool value) { AyuSettings::ghost(session).setSendOnlinePackets(!value); }));
	result.push_back(NativeBoolean(u"ghostDontSendTyping"_q, tr::ayu_DontSendUploadProgress(tr::now),
		[=] { return !AyuSettings::ghost(session).sendUploadProgress(); },
		[=](bool value) { AyuSettings::ghost(session).setSendUploadProgress(!value); }));
	return result;
}

QString CategoryTitle(Category category) {
	switch (category) {
	case Category::Appearance: return tr::lng_regram_backup_appearance(tr::now);
	case Category::Messages: return tr::lng_regram_backup_messages(tr::now);
	case Category::Privacy: return tr::lng_regram_backup_privacy(tr::now);
	case Category::Translation: return tr::lng_regram_backup_translation(tr::now);
	case Category::Media: return tr::lng_regram_backup_media(tr::now);
	case Category::Notifications: return tr::lng_regram_backup_notifications(tr::now);
	}
	Unexpected("Unknown backup category.");
}

void AddCategories(not_null<Ui::GenericBox*> box, not_null<Selection*> state) {
	for (const auto category : kAllCategories) {
		const auto checkbox = box->addRow(object_ptr<Ui::Checkbox>(
			box, CategoryTitle(category), true, st::defaultCheckbox));
		checkbox->checkedValue() | rpl::on_next([=](bool checked) {
			if (checked) {
				state->categories.insert(category);
			} else {
				state->categories.erase(category);
			}
			state->any = !state->categories.empty();
		}, checkbox->lifetime());
	}
}

void TrackSelection(
		not_null<Ui::GenericBox*> box,
		not_null<Selection*> state,
		QPointer<Ui::RoundButton> button) {
	state->any.value() | rpl::on_next([=](bool any) {
		if (button) {
			button->setDisabled(!any);
		}
	}, box->lifetime());
}

void BindSession(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	session->lifetime().add([weak = base::weak_qptr<Ui::GenericBox>(box)] {
		if (weak) {
			weak->closeBox();
		}
	});
}

QJsonObject CurrentValues(not_null<Main::Session*> session) {
	const auto raw = Nagram::ForAccount(session).Get(kSource);
	auto result = Parse(raw).values;
	const auto projection = QJsonDocument::fromJson(raw).object().value(u"desktopProjection"_q).toObject();
	for (const auto &binding : Bindings(session)) {
		const auto value = binding.read();
		if (!value.isNull() && !value.isUndefined()
			&& (!projection.contains(binding.key) || projection.value(binding.key) != value)) {
			if (binding.key == u"chatPreferences"_q) {
				auto merged = QJsonDocument::fromJson(result.value(binding.key).toString().toUtf8()).object();
				const auto account = ToIosPeerId(Address(session->user()->id));
				if (account) {
					const auto prefix = QString::number(*account) + u':';
					for (auto i = merged.begin(); i != merged.end();) {
						i = i.key().startsWith(prefix) ? merged.erase(i) : std::next(i);
					}
				}
				const auto current = QJsonDocument::fromJson(value.toString().toUtf8()).object();
				for (auto i = current.begin(); i != current.end(); ++i) {
					merged.insert(i.key(), i.value());
				}
				result.insert(binding.key, QString::fromUtf8(QJsonDocument(merged).toJson(QJsonDocument::Compact)));
			} else {
				result.insert(binding.key, value);
			}
		}
	}
	return result;
}

void SaveFile(not_null<Ui::GenericBox*> box, QByteArray bytes) {
	if (bytes.isEmpty()) {
		box->showToast(tr::lng_regram_backup_invalid(tr::now));
		return;
	}
	FileDialog::GetWritePath(Core::App().getFileDialogParent(),
		tr::lng_nagram_config_export(tr::now), tr::lng_nagram_config_file_filter(tr::now),
		u"regram-settings.json"_q, crl::guard(box, [=](QString &&path) {
			if (path.isEmpty()) {
				return;
			}
			auto file = QSaveFile(path);
			if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
				box->showToast(tr::lng_nagram_config_write_error(tr::now));
			} else {
				box->showToast(tr::lng_nagram_config_exported(tr::now));
			}
		}));
}

void Preview(
		not_null<Window::SessionController*> controller,
		QJsonObject selected,
		std::set<Category> reset = {}) {
	const auto session = &controller->session();
	auto changes = std::vector<Change>();
	auto unsupported = QStringList();
	const auto bindings = Bindings(session, selected);
	for (auto i = selected.begin(); i != selected.end(); ++i) {
		const auto found = ranges::find(bindings, i.key(), &Binding::key);
		auto change = found == bindings.end() ? std::nullopt : found->prepare(i.value());
		if (change) {
			change->preview = ValueText(found->read(), i.key()) + u" → "_q
				+ ValueText(i.value(), i.key());
			changes.push_back(*change);
		} else {
			unsupported.push_back(i.key());
		}
	}
	const auto beforeSource = Nagram::ForAccount(session).Get(kSource);
	auto projection = QJsonDocument::fromJson(beforeSource).object().value(u"desktopProjection"_q).toObject();
	for (auto i = selected.begin(); i != selected.end(); ++i) {
		const auto binding = ranges::find(bindings, i.key(), &Binding::key);
		if (binding != bindings.end() && unsupported.contains(i.key())) {
			projection.insert(i.key(), binding->read());
		} else {
			projection.remove(i.key());
		}
	}
	auto retained = Parse(beforeSource).values;
	for (auto i = retained.begin(); i != retained.end();) {
		const auto category = CategoryFor(i.key());
		if (category && reset.contains(*category)) {
			projection.remove(i.key());
			i = retained.erase(i);
		} else {
			++i;
		}
	}
	for (auto i = selected.begin(); i != selected.end(); ++i) {
		retained.insert(i.key(), i.value());
	}
	const auto encoded = Encode(retained);
	auto root = QJsonDocument::fromJson(encoded).object();
	root.insert(u"desktopProjection"_q, projection);
	const auto source = QJsonDocument(root).toJson(QJsonDocument::Compact);
	if (encoded.isEmpty() || !ValidateSource(source)) {
		controller->showToast(tr::lng_regram_backup_invalid(tr::now));
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		BindSession(box, session);
		box->setTitle(tr::lng_nagram_config_preview());
		box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_regram_backup_about(), st::boxLabel));
		for (const auto &change : changes) {
			box->addRow(object_ptr<Ui::FlatLabel>(box,
				change.title + u"\n"_q + change.preview, st::boxLabel));
		}
		if (!unsupported.isEmpty()) {
			box->addRow(object_ptr<Ui::FlatLabel>(box,
				tr::lng_regram_backup_unsupported(tr::now, lt_amount, QString::number(unsupported.size()))
					+ u"\n"_q + unsupported.join(u'\n'), st::boxLabel));
		}
		box->addButton(tr::lng_nagram_config_apply(), crl::guard(controller, [=] {
			if (Nagram::ForAccount(session).Get(kSource) != beforeSource
				|| !ranges::all_of(changes, [](const Change &change) { return change.current(); })) {
				box->showToast(tr::lng_regram_backup_changed(tr::now));
				return;
			}
			for (const auto &change : changes) {
				change.apply();
			}
			Expects(Nagram::ForAccount(session).Set(kSource, source));
			Local::writeSettings();
			const auto restart = ranges::any_of(changes, [](const Change &change) { return change.restart; });
			const auto weak = base::make_weak(controller);
			box->closeBox();
			if (const auto strong = weak.get()) {
				strong->showToast(tr::lng_nagram_config_imported(tr::now));
				if (restart) {
					Nagram::ShowRestartPrompt(strong);
				}
			}
		}));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace

void ExportBox(not_null<Ui::GenericBox*> box, not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	BindSession(box, session);
	box->setTitle(tr::lng_regram_cross_platform_backup());
	const auto state = box->lifetime().make_state<Selection>();
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_regram_backup_about(), st::boxLabel));
	AddCategories(box, state);
	const auto exportButton = box->addButton(tr::lng_nagram_config_export(), crl::guard(session, [=] {
		if (!state->categories.empty()) {
			SaveFile(box, Encode(Select(CurrentValues(session), state->categories)));
		}
	}));
	TrackSelection(box, state, exportButton);
	const auto resetButton = box->addButton(tr::lng_regram_backup_reset(), crl::guard(controller, [=] {
		if (state->categories.empty()) {
			return;
		}
		const auto categories = state->categories;
		box->uiShow()->showBox(Ui::MakeConfirmBox({
			.text = tr::lng_regram_backup_reset_confirm(),
			.confirmed = crl::guard(box, [=](Fn<void()> close) {
				auto values = QJsonObject();
				for (const auto &binding : Bindings(session)) {
					const auto category = CategoryFor(binding.key);
					const auto value = binding.defaults ? binding.defaults() : QJsonValue();
					if (category && categories.contains(*category) && !value.isNull()) {
						values.insert(binding.key, value);
					}
				}
				close();
				Preview(controller, std::move(values), categories);
			}),
		}));
	}));
	TrackSelection(box, state, resetButton);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ShowImport(not_null<Window::SessionController*> controller, QByteArray bytes) {
	const auto parsed = Parse(bytes);
	if (!parsed.error.isEmpty()) {
		controller->showToast(tr::lng_regram_backup_invalid(tr::now));
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		BindSession(box, &controller->session());
		box->setTitle(tr::lng_regram_cross_platform_backup());
		const auto state = box->lifetime().make_state<Selection>();
		AddCategories(box, state);
		const auto previewButton = box->addButton(tr::lng_nagram_config_preview(), crl::guard(controller, [=] {
			if (!state->categories.empty()) {
				Preview(controller, Select(parsed.values, state->categories));
				box->closeBox();
			}
		}));
		TrackSelection(box, state, previewButton);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Regram::Settings::IosBackup
