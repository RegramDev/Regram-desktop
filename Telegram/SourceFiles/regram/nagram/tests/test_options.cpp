#include "regram/nagram/core/options.h"
#include "regram/nagram/core/exchange.h"
#include "regram/nagram/core/device_options.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/interface/main_menu.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/media/options.h"
#include "regram/nagram/menu/model.h"
#include "regram/nagram/filters/model.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/messages/time_format.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

class MemoryPrefs final : public Nagram::RawPrefs {
public:
	[[nodiscard]] QByteArray read(std::string_view key) override {
		const auto found = values.find(std::string(key));
		return (found == values.end()) ? QByteArray() : found->second;
	}
	void write(std::string_view key, const QByteArray &value) override {
		values[std::string(key)] = value;
	}
	void clear(std::string_view key) override {
		values.erase(std::string(key));
	}

	std::map<std::string, QByteArray> values;
};

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

[[nodiscard]] bool ValidPercent(const int &value) {
	return value >= 0 && value <= 100;
}

auto ValidationCalls = 0;

bool ValidCachedObject(const QByteArray &value) {
	++ValidationCalls;
	return value == "valid";
}

bool ValidCachedString(const QString &value) {
	++ValidationCalls;
	return value == u"valid"_q;
}

void TestValidationCache() {
	using namespace Nagram;
	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	const auto object = Option<QByteArray>{
		"nagram.cachedObject", Scope::Device, {},
		Category::Rules, "lng_nagram_filter_rules", 0, ValidCachedObject,
	};
	const auto text = Option<QString>{
		"nagram.cachedString", Scope::Device, {},
		Category::Rules, "lng_nagram_filter_rules", 0, ValidCachedString,
	};
	prefs.write(object.key, "valid");
	prefs.write(text.key, "svalid");
	ValidationCalls = 0;
	for (auto i = 0; i != 100; ++i) {
		Require(options.Get(object) == "valid"
			&& options.Get(text) == u"valid"_q, "cached settings changed");
	}
	Require(ValidationCalls == 2, "unchanged settings were revalidated");
	prefs.write(object.key, "invalid");
	prefs.write(text.key, "sinvalid");
	Require(options.Get(object).isEmpty() && options.Get(text).isEmpty(),
		"external invalid settings were hidden by the cache");
	Require(options.invalidKeys().size() == 2,
		"invalid settings lost their diagnostics");
	prefs.write(object.key, "valid");
	Require(options.Get(object) == "valid"
		&& !options.invalidKeys().contains(object.key),
		"repaired setting remained invalid");
	Require(options.Set(text, u"valid"_q)
		&& options.Get(text) == u"valid"_q,
		"setting write did not invalidate validation");
	std::cout << "PASS: Regram settings validation cache" << std::endl;
}

} // namespace

void TestOptions() {
	TestValidationCache();
	using namespace Nagram;
	using Menu::ActionId;
	using Menu::Visibility;
	const auto hiddenReply = Menu::WriteVisibility(
		{}, ActionId::Reply, Visibility::Hide);
	Require(Menu::ValidateConfig(hiddenReply), "menu config valid");
	Require(Menu::ReadVisibility(hiddenReply, ActionId::Reply)
		== Visibility::Hide, "menu hidden state");
	Require(Menu::ReadVisibility(hiddenReply, ActionId::Edit)
		== Visibility::Show, "menu default state");
	const auto optionReply = Menu::WriteVisibility(
		hiddenReply, ActionId::Reply, Visibility::WithOption);
	Require(!Menu::Visible(Menu::ReadVisibility(optionReply, ActionId::Reply),
		false), "menu option released");
	Require(Menu::Visible(Menu::ReadVisibility(optionReply, ActionId::Reply),
		true), "menu option held");
	Require(Menu::WriteVisibility(optionReply, ActionId::Reply,
		Visibility::Show).isEmpty(), "menu default removes stored override");
	Require(!Menu::ValidateConfig(R"({"version":1,"states":{"E99":"hide"}})"),
		"unknown menu action accepted");
	Require(!Menu::ValidateConfig(R"({"version":1,"states":{"E01":"unknown"}})"),
		"invalid menu state accepted");
	Require(Menu::ReadVisibility({}, ActionId::Repeat) == Visibility::Hide,
		"new menu action default");
	Require(Menu::ReadVisibility({}, ActionId::Reading) == Visibility::Hide,
		"reading menu action default");
	Require(Menu::ReadVisibility({}, ActionId::DeleteDownload)
		== Visibility::Hide, "delete download action default");
	Require(Menu::ValidateConfig(R"({"version":2,"states":{"E25":"show"}})"),
		"delete download action state rejected");
	Require(Menu::ReadVisibility({}, ActionId::QuickRating)
		== Visibility::Hide, "quick rating action default");
	Require(Menu::ValidQuickRating(QString(64, u'x'))
		&& !Menu::ValidQuickRating(QString(65, u'x')),
		"quick rating length limit");
	Require(Menu::ReadVisibility({}, ActionId::SelectBetween)
		== Visibility::Hide, "select between action default");
	Require(Menu::ReadVisibility({}, ActionId::SeenBy) == Visibility::Show
		&& Menu::IsUpstream(ActionId::SeenBy), "seen by stays upstream");
	Require(Menu::ReadVisibility({}, ActionId::TranscribeSelected)
			== Visibility::Hide
		&& !Menu::IsUpstream(ActionId::TranscribeSelected)
		&& Menu::ValidateConfig(Menu::WriteVisibility(
			{}, ActionId::TranscribeSelected, Visibility::Show)),
		"transcribe selected is hidden by default");
	Require(Menu::ReadVisibility({}, ActionId::Summarize) == Visibility::Hide
		&& !Menu::IsUpstream(ActionId::Summarize)
		&& Menu::ValidateConfig(Menu::WriteVisibility(
			{}, ActionId::Summarize, Visibility::Show)),
		"summarize is hidden by default");
	Require(Menu::ReadVisibility({}, ActionId::CopyMarkdown) == Visibility::Hide
		&& !Menu::IsUpstream(ActionId::CopyMarkdown)
		&& Menu::ValidateConfig(R"({"version":2,"states":{"E39":"show"}})"),
		"copy as Markdown is hidden by default");
	Require(Menu::ReadVisibility({}, ActionId::MessageDetails) == Visibility::Hide
		&& Menu::ReadVisibility({}, ActionId::SelectAll) == Visibility::Hide,
		"message tool actions default");
	const auto bar = Menu::Slot{ .separator = true };
	const auto info = Menu::Slot();
	const auto at = [](ActionId id) { return Menu::Slot{ .id = id }; };
	Require(Menu::EndPosition({ at(ActionId::Reply), at(ActionId::Delete),
		at(ActionId::Select), bar, info }) == 1, "menu end before delete");
	Require(Menu::EndPosition({ at(ActionId::Reply), at(ActionId::Select),
		bar, at(ActionId::SeenBy), bar, info }) == 2,
		"menu end above the info footer");
	Require(Menu::EndPosition({ at(ActionId::Reply),
		at(ActionId::SaveToSaved), bar, info }) == 2,
		"menu end after an inserted action");
	Require(Menu::EndPosition({ at(ActionId::Reply), info }) == 2
		&& Menu::EndPosition({ info, bar, info }) == 3,
		"menu end without a footer");
	Require(Filters::ValidHiddenMessages(QString::fromLatin1("5:10,6:2"))
		&& !Filters::ValidHiddenMessages(QString::fromLatin1("5:-1"))
		&& !Filters::ValidHiddenMessages(QString::fromLatin1("x:1")),
		"hidden messages format");
	const auto oldReading = QByteArray(
		R"({"version":1,"states":{"E21":"show","E23":"show"}})");
	Require(Menu::ValidateConfig(oldReading), "old menu config valid");
	Require(Menu::ReadVisibility(oldReading, ActionId::Reading)
		== Visibility::Show, "old reading visibility");
	Require(Menu::ReadVisibility(oldReading, ActionId::Screenshot)
		== Visibility::Hide, "old config screenshot default");
	const auto migrated = Menu::WriteVisibility(
		oldReading, ActionId::Screenshot, Visibility::Show);
	Require(Menu::ReadVisibility(migrated, ActionId::Reading)
		== Visibility::Show, "reading visibility migrated");
	Require(Menu::ReadVisibility(migrated, ActionId::Screenshot)
		== Visibility::Show, "screenshot visibility saved");
	Require(Menu::ReadVisibility(migrated, ActionId::FilterAuthor)
		== Visibility::Show, "filter author visibility preserved");
	const auto shownRepeat = Menu::WriteVisibility({}, ActionId::Repeat,
		Visibility::Show);
	Require(Menu::ReadVisibility(shownRepeat, ActionId::Repeat)
		== Visibility::Show, "new menu action override");
	Require(!Menu::ValidateConfig(R"({"version":1,"states":{},"extra":1})"),
		"unknown menu field accepted");
	const auto option = Option<int>{
		"nagram.testPercent", Scope::Device, 0, Category::Interface,
		"lng_nagram_test_percent", 0, ValidPercent };
	auto registry = Registry();
	Require(registry.Add(option), "register option");
	Require(!registry.Add(option), "duplicate key accepted");
	Require(registry.All().size() == 1, "registry count");
	auto messages = Registry();
	Messages::RegisterOptions(messages);
	Require(messages.All().size() == 33, "message option count");
	auto refreshCount = 0;
	for (const auto &entry : messages.All()) {
		Require(entry.scope == Scope::Device, "message option scope");
		Require(entry.category == Category::Messages, "message option category");
		refreshCount += messages.HasFlag(entry.key, Flag::RefreshMessageView);
	}
	Require(refreshCount == 24, "message refresh option count");
	Require(Messages::kReadingChinese.validate(0)
		&& Messages::kReadingChinese.validate(1)
		&& Messages::kReadingChinese.validate(2)
		&& !Messages::kReadingChinese.validate(3),
		"reading conversion modes");
	Require(messages.HasFlag(Messages::kSecondsInMessages.key,
		Flag::RefreshMessageView), "message refresh option missing");
	Require(!messages.HasFlag(Messages::kHideReactionMenu.key,
		Flag::RefreshMessageView), "unrelated refresh option");
	Require(!messages.HasFlag("nagram.unknown", Flag::RefreshMessageView),
		"unknown refresh option");
	auto chats = Registry();
	Chats::RegisterOptions(chats);
	Require(chats.All().size() == 33, "chat option count");
	Require(Chats::ValidReadingPositions(QString::fromLatin1("5:10,7:1"))
		&& !Chats::ValidReadingPositions(QString::fromLatin1("5:0"))
		&& !Chats::ValidReadingPositions(QString::fromLatin1("5"))
		&& chats.HasFlag(Chats::kReadingPositions.key, Flag::Hidden),
		"reading positions format");
	Require(Chats::ValidRecentChats(QString())
		&& Chats::ValidRecentChats(QString::fromLatin1("1,2,3"))
		&& !Chats::ValidRecentChats(QString::fromLatin1("1,,2"))
		&& !Chats::ValidRecentChats(QString::fromLatin1("1,x")),
		"recent chats format");
	Require(chats.HasFlag(Chats::kRecentChatsList.key, Flag::Hidden)
		&& !chats.HasFlag(Chats::kRecentChatsList.key, Flag::Exportable),
		"recent chats list stays local");
	Require(Chats::kManagedFolderIds.scope == Scope::Account
		&& Chats::kManagedFolderIds.validate(QString::fromLatin1("1,3,8"))
		&& !Chats::kManagedFolderIds.validate(QString::fromLatin1("3,1"))
		&& !Chats::kManagedFolderIds.validate(QString::fromLatin1("1,1")),
		"managed folder ids must be account scoped and unique");
	Require(Chats::kChatSort.validate(0)
		&& Chats::kChatSort.validate(0xE41)
		&& !Chats::kChatSort.validate(1)
		&& !Chats::kChatSort.validate(0x1000),
		"chat sort encoding");
	Require(Chats::kStartupFolderMode.scope == Scope::Account
		&& Chats::kStartupFolderId.scope == Scope::Account
		&& Chats::kLastOpenedFolderId.scope == Scope::Account,
		"startup folder must be account scoped");
	Require(chats.HasFlag(Chats::kCompactList.key,
		Flag::RefreshDialogList), "compact list refresh flag");
	Require(chats.HasFlag(Chats::kPreviewLines.key,
		Flag::RefreshDialogList), "preview line refresh flag");
	Require(!chats.HasFlag(Chats::kHideStories.key,
		Flag::RefreshDialogList), "stories use widget refresh");
	auto interface = Registry();
	Interface::RegisterOptions(interface);
	Require(interface.All().size() == 19, "interface option count");
	Require(interface.HasFlag(Interface::kHalfwidthUiPunctuation.key,
		Flag::RequiresRestart), "interface text restart flag");
	Require(interface.HasFlag(Interface::kBubbleRoundness.key,
		Flag::RequiresRestart), "roundness restart flag");
	Require(Interface::kAvatarRoundness.validate(0)
		&& Interface::kAvatarRoundness.validate(10)
		&& Interface::kAvatarRoundness.validate(100)
		&& !Interface::kAvatarRoundness.validate(9)
		&& !Interface::kAvatarRoundness.validate(101),
		"avatar roundness bounds");
	Require(Interface::kTextMessageWidth.validate(0)
		&& Interface::kTextMessageWidth.validate(50)
		&& Interface::kTextMessageWidth.validate(400)
		&& !Interface::kTextMessageWidth.validate(49)
		&& !Interface::kTextMessageWidth.validate(401),
		"text width bounds");
	Require(interface.HasFlag(Interface::kHideReplyThumbnail.key,
		Flag::RefreshMessageView), "reply thumbnail refresh flag");
	Require(Interface::ValidMainMenuBytes({}), "default menu config");
	Require(Interface::ValidMainMenu(Interface::MainMenuDefaults()),
		"default menu config invalid");
	Require(!Interface::ValidMainMenuBytes(R"({"version":1,"order":[],"hidden":["settings"],"title":"","seasonalDecorations":true})"),
		"settings cannot be hidden");
	Require(!Interface::ValidMainMenuBytes(R"({"version":1,"order":["calls","calls"],"hidden":[],"title":"","seasonalDecorations":true})"),
		"duplicate menu action accepted");
	Require(Interface::kNotificationDelay.validate(0)
		&& Interface::kNotificationDelay.validate(500)
		&& Interface::kNotificationDelay.validate(60000)
		&& !Interface::kNotificationDelay.validate(400)
		&& !Interface::kNotificationDelay.validate(61000),
		"notification delay bounds");
	auto compose = Registry();
	Compose::RegisterOptions(compose);
	Require(compose.All().size() == 31, "compose option count");
	Require(Compose::kDefaultCodeLanguage.validate(QString::fromLatin1("cpp"))
		&& !Compose::kDefaultCodeLanguage.validate(QString::fromLatin1("c++!")),
		"code language validation");
	Require(Compose::kQuickReplies.validate(
		R"({"version":1,"replies":["one","two"]})")
		&& !Compose::kQuickReplies.validate(
			R"({"version":1,"replies":["one"]})"),
		"quick reply schema");
	for (const auto &entry : compose.All()) {
		if (entry.key != Compose::kDisableEmojiHover.key
			&& entry.key != Compose::kDisableAttachHover.key
			&& entry.key != Compose::kBotCommandsToDraft.key
			&& entry.key != Compose::kInputPlaceholderMode.key
			&& entry.key != Compose::kDisableAutoMarkdown.key
			&& entry.key != Compose::kFormatToolbar.key
			&& entry.key != Compose::kHiddenFormatItems.key
			&& entry.key != Compose::kInputChinese.key
			&& entry.key != Compose::kDisableLinkPreview.key
			&& entry.key != Compose::kSpaceOnSend.key
			&& entry.key != Compose::kSpaceOnEdit.key
			&& entry.key != Compose::kDefaultCodeLanguage.key
			&& entry.key != Compose::kQuickReplies.key
			&& entry.key != Compose::kConfirmSticker.key
			&& entry.key != Compose::kConfirmGif.key
			&& entry.key != Compose::kPreviewVoice.key
			&& entry.key != Compose::kPreviewRoundVideo.key
			&& entry.key != Compose::kConfirmPrivateCall.key
			&& entry.key != Compose::kForwardBeforeComment.key) {
			Require(compose.HasFlag(entry.key, Flag::RefreshComposeButtons),
				"compose button refresh flag");
		}
	}
	auto media = Registry();
	Media::RegisterOptions(media);
	Require(media.All().size() == 16, "media option count");
	Require(media.HasFlag(Media::kStickerScale.key,
		Flag::RefreshMessageView), "sticker scale refresh flag");
	Require(!media.HasFlag(Media::kRecentStickerLimit.key,
		Flag::RefreshMessageView), "recent sticker uses panel refresh");
	Require(media.HasFlag(Media::kDisableVideoAutoplay.key,
		Flag::RefreshMessageView), "video autoplay refresh flag");
	auto privacy = Registry();
	Privacy::RegisterOptions(privacy);
	Require(privacy.All().size() == 16, "privacy option count");
	Require(Privacy::kProfileIdFormat.validate(0)
		&& Privacy::kProfileIdFormat.validate(1)
		&& Privacy::kProfileIdFormat.validate(2)
		&& !Privacy::kProfileIdFormat.validate(3),
		"profile id format bounds");
	const auto locale = QLocale();
	QLocale::setDefault(QLocale::c());
	Require(Messages::FormatTime(QTime(9, 8, 7), false)
		== QString::fromLatin1("09:08"),
		"default time format");
	const auto withSeconds = Messages::FormatTime(QTime(9, 8, 7), true);
	if (withSeconds != QString::fromLatin1("09:08:07")) {
		throw std::runtime_error(
			"seconds time format: " + withSeconds.toStdString());
	}
	QLocale::setDefault(locale);

	auto prefs = MemoryPrefs();
	auto &subscriber = details::SharedDeviceOptions(prefs);
	auto &writer = details::SharedDeviceOptions(prefs);
	Require(&subscriber == &writer, "device entries use different instances");
	auto &options = subscriber;
	Require(options.Get(option) == 0, "default value");
	auto changes = std::vector<int>();
	auto lifetime = rpl::lifetime();
	subscriber.Value(option) | rpl::on_next([&](int value) {
		changes.push_back(value);
	}, lifetime);
	Require(changes == std::vector{ 0 }, "initial notification");
	Require(writer.Set(option, 42), "valid write");
	Require(prefs.values["nagram.testPercent"] == "42", "stored value");
	Require(options.Get(option) == 42, "round trip");
	Require(options.Set(option, 42), "repeat write");
	Require(changes == (std::vector{ 0, 42 }), "duplicate notification");
	Require(!options.Set(option, 101), "invalid value accepted");
	Require(options.Get(option) == 42, "invalid value changed storage");

	prefs.values["nagram.testPercent"] = "broken";
	Require(options.Get(option) == 0, "invalid stored value fallback");
	Require(prefs.values["nagram.testPercent"] == "broken",
		"invalid payload overwritten");
	Require(options.invalidKeys().contains(option.key), "read error absent");
	Require(options.Set(option, 0), "clear to default");
	Require(!prefs.values.contains("nagram.testPercent"), "default not cleared");
	Require(options.invalidKeys().empty(), "stale read error");
	Require(changes == (std::vector{ 0, 42, 0 }), "clear notification");

	const auto text = Option<QString>{
		"nagram.testText", Scope::Device, QString::fromUtf8("default"),
		Category::Interface, "lng_nagram_test_text" };
	Require(options.Set(text, QString()), "empty string write");
	Require(options.Get(text).isEmpty(), "empty string round trip");
	Require(!options.Set(text, QString::fromUtf8("two\nlines")),
		"multiline string accepted");
	std::cout << "PASS: Nagram device options" << std::endl;

	const auto accountOption = Option<bool>{
		"nagram.testAccount", Scope::Account, false, Category::Chats,
		"lng_nagram_test_account" };
	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	Require(first.Set(accountOption, true), "account write");
	Require(first.Get(accountOption), "first account value");
	Require(!second.Get(accountOption), "account values leaked");
	Require(!options.Set(accountOption, true), "device accepted account option");
	std::cout << "PASS: Nagram account options" << std::endl;

	auto exchangeRegistry = Registry();
	Require(exchangeRegistry.Add(option), "exchange int registration");
	Require(exchangeRegistry.Add(accountOption), "exchange account registration");
	Menu::RegisterOptions(exchangeRegistry);
	auto exchangePrefs = MemoryPrefs();
	auto exchangeOptions = Options(exchangePrefs);
	Require(exchangeOptions.Set(option, 42), "exchange int setup");
	Require(exchangeOptions.Set(Menu::kMenuConfig, hiddenReply),
		"exchange menu setup");
	const auto exported = Exchange::Export(exchangeOptions, exchangeRegistry);
	Require(exported.invalidKeys.empty(), "unexpected export error");
	const auto exportedValues = QJsonDocument::fromJson(exported.data)
		.object().value("options").toObject();
	Require(exportedValues.value("nagram.testPercent") == 42,
		"integer missing from export");
	Require(exportedValues.value("nagram.messageMenu").isObject(),
		"structured menu missing from export");
	Require(!exportedValues.contains("nagram.testAccount"),
		"account value included in device export");

	const auto payload = QByteArray(R"({"version":1,"options":{"nagram.testPercent":25,"nagram.messageMenu":{"version":1,"states":{"E01":"option"}},"nagram.future":true}})");
	const auto plan = Exchange::PlanImport(
		exchangeOptions, exchangeRegistry, payload);
	Require(plan.error.isEmpty() && plan.changes.size() == 2,
		"valid import preview");
	Require(plan.skippedKeys == QStringList{ QString::fromLatin1("nagram.future") },
		"unknown key was not skipped");
	auto seen = std::vector<std::pair<int, Menu::Visibility>>();
	auto exchangeLifetime = rpl::lifetime();
	exchangeOptions.changes() | rpl::on_next([&](auto) {
		seen.emplace_back(exchangeOptions.Get(option),
			Menu::ReadVisibility(exchangeOptions.Get(Menu::kMenuConfig),
				ActionId::Reply));
	}, exchangeLifetime);
	const auto applied = Exchange::Apply(
		exchangeOptions, exchangeRegistry, plan);
	Require(applied.applied && applied.error.isEmpty(), "valid import apply");
	Require(exchangeOptions.Get(option) == 25 && seen.size() == 2,
		"import values or notifications");
	for (const auto &[number, visibility] : seen) {
		Require(number == 25 && visibility == Visibility::WithOption,
			"partial import observed");
	}

	const auto invalidPayload = QByteArray(R"({"version":1,"options":{"nagram.testPercent":99,"nagram.messageMenu":{"version":1,"states":{"E01":"bad"}}}})");
	const auto invalid = Exchange::PlanImport(
		exchangeOptions, exchangeRegistry, invalidPayload);
	Require(!invalid.error.isEmpty() && invalid.changes.empty(),
		"invalid object produced an import plan");
	Require(exchangeOptions.Get(option) == 25,
		"invalid import changed storage");
	Require(!Exchange::PlanImport(exchangeOptions, exchangeRegistry,
		R"({"version":"1","options":{}})").error.isEmpty(),
		"string version accepted");
	Require(!Exchange::PlanImport(exchangeOptions, exchangeRegistry,
		R"({"version":1,"options":{"nagram.testPercent":25.5}})")
		.error.isEmpty(), "fractional integer accepted");
	const auto stale = Exchange::PlanImport(exchangeOptions, exchangeRegistry,
		R"({"version":1,"options":{"nagram.testPercent":30}})");
	Require(exchangeOptions.Set(option, 20), "conflict setup");
	Require(!Exchange::Apply(exchangeOptions, exchangeRegistry, stale).applied,
		"stale import applied");
	Require(exchangeOptions.Get(option) == 20, "stale import changed storage");
	Require(Interface::ValidAppIcon(QString()), "default app icon rejected");
	Require(Interface::ValidAppIcon(QString::fromLatin1("block_niello")),
		"app icon rejected");
	Require(!Interface::ValidAppIcon(QString::fromLatin1("../block")),
		"app icon path accepted");
	Require(!Interface::ValidAppIcon(QString::fromLatin1("Block")),
		"unknown app icon accepted");
	std::cout << "PASS: Nagram settings exchange" << std::endl;
}
