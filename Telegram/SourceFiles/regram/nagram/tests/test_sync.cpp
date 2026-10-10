#include "regram/nagram/chats/local_pins_model.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/core/diagnostics.h"
#include "regram/nagram/core/exchange.h"
#include "regram/nagram/filters/model.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/links/inline_rules.h"
#include "regram/nagram/links/model.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/links/webview.h"
#include "regram/nagram/media/backend_options.h"
#include "regram/nagram/media/local_faved_model.h"
#include "regram/nagram/media/options.h"
#include "regram/nagram/menu/model.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/network/options.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/services/auto_translate_model.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/snapshot/cloud_theme_model.h"
#include "regram/nagram/sync/model.h"

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

constexpr auto kUser = quint64(777000111);
constexpr auto kOtherUser = quint64(777000222);

void TestCloudThemeRef() {
	using namespace Nagram;
	using namespace Nagram::Snapshot;

	const auto ref = CloudThemeRef{
		.user = kUser,
		.themeId = 0xFFFFFFFFFFFFFFF0ULL,
		.accessHash = 0x8000000000000001ULL,
		.documentId = 5,
		.title = QString::fromUtf8("Night\nsky"),
		.slug = QString::fromLatin1("night"),
	};
	const auto raw = SerializeCloudThemeRef(ref);
	const auto parsed = ParseCloudThemeRef(raw);
	Require(parsed
		&& parsed->user == kUser
		&& parsed->themeId == ref.themeId
		&& parsed->accessHash == ref.accessHash
		&& parsed->documentId == 5
		&& parsed->title == QString::fromLatin1("Night sky")
		&& parsed->slug == ref.slug,
		"cloud theme reference round trip");
	Require(ValidCloudThemeRef(raw) && ValidCloudThemeRef(QByteArray()),
		"cloud theme reference accepted");

	const auto with = [&](const char *key, const QJsonValue &value) {
		auto object = QJsonDocument::fromJson(raw).object();
		object.insert(QString::fromLatin1(key), value);
		return QJsonDocument(object).toJson(QJsonDocument::Compact);
	};
	const auto without = [&](const char *key) {
		auto object = QJsonDocument::fromJson(raw).object();
		object.remove(QString::fromLatin1(key));
		return QJsonDocument(object).toJson(QJsonDocument::Compact);
	};
	for (const auto key : {
		"version", "user", "themeId", "accessHash", "documentId", "title",
		"slug" }) {
		Require(!ValidCloudThemeRef(without(key)),
			"cloud theme reference with a missing field accepted");
	}
	for (const auto key : { "user", "themeId", "accessHash", "documentId" }) {
		Require(!ValidCloudThemeRef(with(key, QString::fromLatin1("0")))
			&& !ValidCloudThemeRef(with(key, QString::fromLatin1("12x")))
			&& !ValidCloudThemeRef(with(key, QString::fromLatin1("-4")))
			&& !ValidCloudThemeRef(with(key, QString::fromLatin1("007")))
			&& !ValidCloudThemeRef(with(key, 12)),
			"cloud theme reference with a bad id accepted");
	}
	Require(!ValidCloudThemeRef(with("title",
			QString(kCloudThemeTitleLimit + 1, QChar(u'x'))))
		&& ValidCloudThemeRef(with("title",
			QString(kCloudThemeTitleLimit, QChar(u'x'))))
		&& !ValidCloudThemeRef(with("title", QString::fromLatin1("a\nb")))
		&& !ValidCloudThemeRef(with("title", 1)),
		"cloud theme title limits");
	Require(!ValidCloudThemeRef(with("extra", true))
		&& !ValidCloudThemeRef(with("version", 2))
		&& !ValidCloudThemeRef("[]")
		&& !ValidCloudThemeRef("{"),
		"malformed cloud theme reference accepted");

	Require(ValidCloudAccount(QString())
		&& ValidCloudAccount(QString::fromLatin1("777000111"))
		&& ValidCloudAccount(QString::fromLatin1("72057594038705039"))
		&& !ValidCloudAccount(QString::fromLatin1("0"))
		&& !ValidCloudAccount(QString::fromLatin1("0123"))
		&& !ValidCloudAccount(QString::fromLatin1("-5"))
		&& !ValidCloudAccount(QString::fromLatin1("12 "))
		&& !ValidCloudAccount(QString::fromLatin1("abc")),
		"cloud theme account pointer format");

	auto registry = Registry();
	RegisterCloudThemeOptions(registry);
	const auto themeInfo = registry.Find(kCloudTheme.key);
	const auto accountInfo = registry.Find(kCloudAccount.key);
	Require(themeInfo
		&& themeInfo->scope == Scope::Account
		&& themeInfo->fallbackRaw.isEmpty()
		&& !registry.HasFlag(kCloudTheme.key, Flag::Exportable),
		"cloud theme reference must be account data, empty by default");
	Require(accountInfo
		&& accountInfo->scope == Scope::Device
		&& accountInfo->fallbackRaw == "s"
		&& registry.HasFlag(kCloudAccount.key, Flag::Hidden)
		&& !registry.HasFlag(kCloudAccount.key, Flag::Exportable),
		"cloud theme pointer must be hidden device data, empty by default");

	auto devicePrefs = MemoryPrefs();
	auto device = Options(devicePrefs);
	Require(device.Set(kCloudAccount, QString::number(kUser))
		&& !device.Set(kCloudAccount, QString::fromLatin1("me")),
		"cloud theme pointer write");
	const auto exported = Exchange::Export(device, registry);
	Require(exported.invalidKeys.isEmpty()
		&& !exported.data.contains("snapshotCloud")
		&& !exported.data.contains("777000111"),
		"cloud theme pointer leaked into the export");
	const auto plan = Exchange::PlanImport(device, registry,
		R"({"version":1,"options":{"nagram.snapshotCloudAccount":"5","nagram.snapshotCloudTheme":{"version":1}}})");
	Require(plan.error.isEmpty()
		&& plan.changes.empty()
		&& plan.skippedKeys.size() == 2,
		"cloud theme keys must not be importable");

	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	Require(first.Set(kCloudTheme, raw)
		&& !first.Set(kCloudTheme, QByteArray("{}")),
		"cloud theme reference write");
	Require(first.Get(kCloudTheme) == raw
		&& second.Get(kCloudTheme).isEmpty(),
		"cloud theme reference crossed accounts");
	auto other = ref;
	other.user = kOtherUser;
	Require(second.Set(kCloudTheme, SerializeCloudThemeRef(other))
		&& ParseCloudThemeRef(first.Get(kCloudTheme))->user == kUser
		&& ParseCloudThemeRef(second.Get(kCloudTheme))->user == kOtherUser,
		"cloud theme references are kept per account");
	Require(first.Set(kCloudTheme, QByteArray())
		&& first.Get(kCloudTheme).isEmpty()
		&& !second.Get(kCloudTheme).isEmpty(),
		"clearing one account's cloud theme touched another");
}

[[nodiscard]] QByteArray Json(const QJsonObject &object) {
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

[[nodiscard]] QJsonObject Object(const QByteArray &data) {
	return QJsonDocument::fromJson(data).object();
}

void TestEnvelope() {
	using namespace Nagram::Sync;

	const auto exported = QByteArray(
		R"({"version":1,"options":{"nagram.hideStories":true}})");
	const auto reordered = QByteArray(
		"{ \"options\": {\"nagram.hideStories\": true},\n\"version\": 1 }");
	const auto hash = PayloadHash(exported);
	Require(hash.size() == 64
		&& hash == PayloadHash(reordered)
		&& hash != PayloadHash(R"({"version":1,"options":{}})")
		&& PayloadHash("not json").isEmpty(),
		"payload hash must depend on the content only");

	const auto encoded = EncodeEnvelope(
		exported,
		1790000000,
		QString::fromLatin1("7.2.10"));
	const auto decoded = DecodeEnvelope(encoded);
	Require(decoded.error == EnvelopeError::None
		&& decoded.value.updatedAt == 1790000000
		&& decoded.value.app == QString::fromLatin1("7.2.10")
		&& decoded.value.hash == hash
		&& Object(decoded.value.payload) == Object(exported),
		"envelope round trip");
	Require(EncodeEnvelope("[]", 1, QString()).isEmpty(),
		"envelope built from a broken export");

	const auto with = [&](const char *key, const QJsonValue &value) {
		auto object = Object(encoded);
		object.insert(QString::fromLatin1(key), value);
		return Json(object);
	};
	const auto without = [&](const char *key) {
		auto object = Object(encoded);
		object.remove(QString::fromLatin1(key));
		return Json(object);
	};
	const auto error = [](const QByteArray &data) {
		return DecodeEnvelope(data).error;
	};
	Require(error(with("version", 2)) == EnvelopeError::Newer
		&& error(with("version", 99)) == EnvelopeError::Newer,
		"newer envelope must be reported as newer");
	Require(error(with("version", 0)) == EnvelopeError::Damaged
		&& error(with("version", 1.5)) == EnvelopeError::Damaged
		&& error(with("version", QString::fromLatin1("1")))
			== EnvelopeError::Damaged
		&& error(without("version")) == EnvelopeError::Damaged,
		"bad envelope version accepted");
	Require(error(with("kind", QString::fromLatin1("nagram-settings")))
			== EnvelopeError::Foreign
		&& error(with("kind", 1)) == EnvelopeError::Foreign
		&& error(without("kind")) == EnvelopeError::Foreign
		&& error(exported) == EnvelopeError::Foreign
		&& error("{}") == EnvelopeError::Foreign,
		"a file of another kind must be reported as foreign");
	for (const auto key : { "updatedAt", "app", "payload" }) {
		Require(error(without(key)) == EnvelopeError::Damaged,
			"envelope with a missing field accepted");
	}
	Require(error(with("device", QString::fromLatin1("x")))
			== EnvelopeError::Damaged
		&& error(with("updatedAt", -1)) == EnvelopeError::Damaged
		&& error(with("updatedAt", 1.5)) == EnvelopeError::Damaged
		&& error(with("updatedAt", QString::fromLatin1("1")))
			== EnvelopeError::Damaged
		&& error(with("app", 7)) == EnvelopeError::Damaged
		&& error(with("app", QString(33, QChar(u'x'))))
			== EnvelopeError::Damaged
		&& error(with("payload", QString::fromLatin1("x")))
			== EnvelopeError::Damaged,
		"malformed envelope accepted");
	Require(error(with("payload", Object(
			R"({"version":2,"options":{}})"))) == EnvelopeError::Newer
		&& error(with("payload", Object(
			R"({"version":1,"options":[]})"))) == EnvelopeError::Damaged
		&& error(with("payload", Object(
			R"({"version":1,"options":{},"extra":1})")))
			== EnvelopeError::Damaged
		&& error(with("payload", Object(R"({"options":{}})")))
			== EnvelopeError::Damaged,
		"malformed payload accepted");
	Require(error("not json") == EnvelopeError::Damaged
		&& error("[]") == EnvelopeError::Damaged
		&& error(QByteArray()) == EnvelopeError::Damaged
		&& error(encoded.left(encoded.size() / 2)) == EnvelopeError::Damaged,
		"damaged envelope accepted");
	Require(error(QByteArray(kMaxBackupBytes + 1, ' '))
			== EnvelopeError::TooLarge,
		"oversized envelope accepted");
}

void TestDecision() {
	using namespace Nagram::Sync;

	const auto a = QByteArray("a");
	const auto b = QByteArray("b");
	const auto c = QByteArray("c");
	const auto none = QByteArray();
	Require(Decide(a, none, none) == Action::Upload
		&& Decide(a, a, none) == Action::Upload
		&& Decide(a, b, none) == Action::Upload,
		"no cloud backup must lead to an upload");
	Require(Decide(a, none, a) == Action::UpToDate
		&& Decide(a, a, a) == Action::UpToDate
		&& Decide(a, b, a) == Action::UpToDate,
		"equal content must be up to date");
	Require(Decide(a, b, b) == Action::Upload,
		"only this device changed");
	Require(Decide(a, a, b) == Action::Download,
		"only the cloud backup changed");
	Require(Decide(a, none, b) == Action::Conflict
		&& Decide(a, c, b) == Action::Conflict,
		"both sides changed or never synced");
}

void TestSupersededBackups() {
	using namespace Nagram::Sync;
	using Ids = std::vector<quint64>;

	Require(SupersededBackups({ 5, 7 }, 7, 9) == Ids{ 5, 7 },
		"the known backup found in the cloud must be removed once");
	Require(SupersededBackups({}, 7, 9) == Ids{ 7 },
		"the known backup must be removed without a cloud listing");
	Require(SupersededBackups({ 5, 9 }, 0, 9) == Ids{ 5 },
		"the uploaded backup must stay");
	Require(SupersededBackups({ 9 }, 9, 9).empty(),
		"replacing a backup in place must remove nothing");
}

void TestState() {
	using namespace Nagram;
	using namespace Nagram::Sync;

	const auto hash = PayloadHash(R"({"version":1,"options":{}})");
	const auto raw = SerializeState({
		.user = kUser,
		.messageId = 42,
		.hash = hash,
		.updatedAt = 1790000000,
	});
	const auto parsed = ParseState(raw);
	Require(parsed
		&& parsed->user == kUser
		&& parsed->messageId == 42
		&& parsed->hash == hash
		&& parsed->updatedAt == 1790000000,
		"sync state round trip");
	Require(ParseState(SerializeState({
			.user = kUser,
			.hash = hash,
		})).has_value(),
		"sync state without a known message");
	const auto with = [&](const char *key, const QJsonValue &value) {
		auto object = Object(raw);
		object.insert(QString::fromLatin1(key), value);
		return Json(object);
	};
	Require(ValidState(QByteArray())
		&& ValidState(raw)
		&& !ValidState(with("version", 2))
		&& !ValidState(with("user", QString::fromLatin1("0")))
		&& !ValidState(with("messageId", 42))
		&& !ValidState(with("hash", QString::fromLatin1("abc")))
		&& !ValidState(with("hash", QString(64, QChar(u'G'))))
		&& !ValidState(with("updatedAt", -5))
		&& !ValidState(with("extra", 1))
		&& !ValidState("{}"),
		"sync state validation");

	auto registry = Registry();
	Sync::RegisterOptions(registry);
	const auto info = registry.Find(kState.key);
	Require(info
		&& info->scope == Scope::Account
		&& info->fallbackRaw.isEmpty()
		&& registry.HasFlag(kState.key, Flag::Hidden)
		&& !registry.HasFlag(kState.key, Flag::Exportable),
		"sync state must be hidden account data, empty by default");

	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	Require(first.Set(kState, raw)
		&& !first.Set(kState, QByteArray("{}"))
		&& first.Get(kState) == raw
		&& second.Get(kState).isEmpty(),
		"sync state crossed accounts");
}

void TestAutoBackup() {
	using namespace Nagram;
	using namespace Nagram::Sync;

	const auto a = QByteArray("a");
	const auto b = QByteArray("b");
	const auto now = qint64(1790000000);
	Require(PlanAuto(now, 0, a, a).step == AutoStep::Skip
		&& PlanAuto(now, now - 5, a, a).step == AutoStep::Skip
		&& PlanAuto(now, 0, QByteArray(), b).step == AutoStep::Skip,
		"unchanged or unreadable settings must not be uploaded");
	Require(PlanAuto(now, 0, a, b).step == AutoStep::Run
		&& PlanAuto(now, 0, a, QByteArray()).step == AutoStep::Run,
		"changed settings without an earlier attempt must run");
	const auto recent = PlanAuto(now, now - 60, a, b);
	Require(recent.step == AutoStep::Wait
		&& recent.wait == kAutoIntervalSeconds - 60,
		"a recent attempt must delay the next one");
	Require(PlanAuto(now, now, a, b).step == AutoStep::Wait
		&& PlanAuto(now, now, a, b).wait == kAutoIntervalSeconds
		&& PlanAuto(now, now - kAutoIntervalSeconds + 1, a, b).wait == 1,
		"automatic backup interval bounds");
	Require(PlanAuto(now, now - kAutoIntervalSeconds, a, b).step
			== AutoStep::Run
		&& PlanAuto(now, now + 3600, a, b).step == AutoStep::Run,
		"an old attempt or a clock set back must not block the backup");
	Require(kAutoDebounceSeconds > 0
		&& kAutoIntervalSeconds >= 10 * kAutoDebounceSeconds
		&& kAutoTimeoutSeconds < kAutoIntervalSeconds,
		"automatic backup timing");

	Require(ValidAutoOwner(QString())
		&& ValidAutoOwner(QString::fromLatin1("777000111"))
		&& !ValidAutoOwner(QString::fromLatin1("0"))
		&& !ValidAutoOwner(QString(30, QChar(u'1')))
		&& !ValidAutoOwner(QString::fromLatin1("on"))
		&& !ValidAutoOwner(QString::fromLatin1("-1")),
		"automatic backup owner format");
	Require(!AutoEnabled(QString(), kUser)
		&& AutoEnabled(QString::number(kUser), kUser)
		&& !AutoEnabled(QString::number(kUser), kOtherUser)
		&& !AutoEnabled(QString::number(kUser), 0)
		&& !AutoEnabled(QString::fromLatin1("1"), kUser),
		"automatic backup must be on only for the user who enabled it");

	auto registry = Registry();
	Sync::RegisterOptions(registry);
	const auto info = registry.Find(kAutoOwner.key);
	Require(info
		&& info->scope == Scope::Account
		&& info->fallbackRaw == "s"
		&& !registry.HasFlag(kAutoOwner.key, Flag::Exportable),
		"automatic backup must be an account option, off by default");

	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	Require(!AutoEnabled(first.Get(kAutoOwner), kUser)
		&& first.Set(kAutoOwner, QString::number(kUser))
		&& !first.Set(kAutoOwner, QString::fromLatin1("yes"))
		&& AutoEnabled(first.Get(kAutoOwner), kUser)
		&& !AutoEnabled(second.Get(kAutoOwner), kOtherUser)
		&& !AutoEnabled(first.Get(kAutoOwner), kOtherUser),
		"automatic backup switch crossed accounts or users");

	auto devicePrefs = MemoryPrefs();
	auto device = Options(devicePrefs);
	devicePrefs.values["nagram.cloudSyncAuto"] = "s777000111";
	const auto exported = Exchange::Export(
		device,
		registry,
		ExchangeTarget::Sync);
	Require(!exported.data.contains("cloudSyncAuto"),
		"the automatic backup switch must not be backed up");
}

void TestAllowlist() {
	using namespace Nagram;
	using namespace Nagram::Sync;

	const auto local = static_cast<unsigned>(Flag::LocalOnly);
	const auto hidden = static_cast<unsigned>(Flag::Hidden);
	const auto shared = Option<bool>{
		"nagram.testShared", Scope::Device, false,
		Category::Interface, "lng_nagram_test_shared" };
	const auto number = Option<int>{
		"nagram.testNumber", Scope::Device, 0,
		Category::Interface, "lng_nagram_test_number" };
	const auto machine = Option<bool>{
		"nagram.testMachine", Scope::Device, false,
		Category::Interface, "lng_nagram_test_machine", local };
	const auto secret = Option<QString>{
		"nagram.testSecret", Scope::Device, QString(),
		Category::Interface, "lng_nagram_test_secret", hidden };
	const auto account = Option<bool>{
		"nagram.testAccountData", Scope::Account, false,
		Category::Interface, "lng_nagram_test_account" };
	auto registry = Registry();
	Require(registry.Add(shared)
		&& registry.Add(number)
		&& registry.Add(machine)
		&& registry.Add(secret)
		&& registry.Add(account),
		"sync test options");
	Sync::RegisterOptions(registry);
	Snapshot::RegisterCloudThemeOptions(registry);
	Require(LocalOnlyKeys(registry)
			== QStringList{ QString::fromLatin1("nagram.testMachine") },
		"local-only keys of the test registry");

	auto prefs = MemoryPrefs();
	auto device = Options(prefs);
	Require(device.Set(shared, true)
		&& device.Set(number, 7)
		&& device.Set(machine, true)
		&& device.Set(secret, QString::fromLatin1("token-123"))
		&& device.Set(
			Snapshot::kCloudAccount,
			QString::number(kUser)),
		"sync test values");
	prefs.values["nagram.cloudSyncState"] = "state-bytes";
	prefs.values["nagram.testAccountData"] = "1";

	const auto file = Exchange::Export(device, registry);
	const auto sync = Exchange::Export(
		device,
		registry,
		ExchangeTarget::Sync);
	const auto fileValues = Object(file.data).value(
		QString::fromLatin1("options")).toObject();
	const auto syncValues = Object(sync.data).value(
		QString::fromLatin1("options")).toObject();
	Require(fileValues.keys() == QStringList{
			QString::fromLatin1("nagram.testMachine"),
			QString::fromLatin1("nagram.testNumber"),
			QString::fromLatin1("nagram.testShared") },
		"local file export keeps local-only keys");
	Require(syncValues.keys() == QStringList{
			QString::fromLatin1("nagram.testNumber"),
			QString::fromLatin1("nagram.testShared") },
		"backup must carry only exportable keys that are not local-only");
	Require(!sync.data.contains("token-123")
		&& !sync.data.contains("state-bytes")
		&& !sync.data.contains("777000111")
		&& !sync.data.contains("cloudSync")
		&& !sync.data.contains("snapshotCloud"),
		"credentials, account data or sync state leaked into the backup");

	const auto envelope = EncodeEnvelope(sync.data, 100, QString());
	const auto decoded = DecodeEnvelope(envelope);
	Require(decoded.error == EnvelopeError::None
		&& decoded.value.hash == PayloadHash(sync.data),
		"backup envelope");

	auto otherPrefs = MemoryPrefs();
	auto other = Options(otherPrefs);
	Require(other.Set(number, 3) && other.Set(machine, false),
		"second device setup");
	const auto before = Exchange::Export(other, registry);
	const auto plan = Exchange::PlanImport(
		other,
		registry,
		decoded.value.payload,
		ExchangeTarget::Sync);
	Require(plan.error.isEmpty()
		&& plan.changes.size() == 2
		&& plan.skippedKeys.isEmpty(),
		"restore preview");
	Require(other.Get(number) == 3 && !other.Get(shared),
		"preview must not change settings");
	Require(Exchange::Apply(other, registry, plan).applied
		&& other.Get(number) == 7
		&& other.Get(shared),
		"restore apply");
	Require(PayloadHash(Exchange::Export(
			other,
			registry,
			ExchangeTarget::Sync).data) == decoded.value.hash,
		"restored device must match the backup");
	const auto again = Exchange::PlanImport(
		other,
		registry,
		decoded.value.payload,
		ExchangeTarget::Sync);
	Require(again.error.isEmpty() && again.changes.empty(),
		"applying the same backup twice must change nothing");

	const auto crafted = QByteArray(R"({"version":1,"options":{"nagram.testMachine":true,"nagram.testSecret":"x","nagram.testAccountData":true,"nagram.cloudSyncState":{"version":1},"nagram.testNumber":9}})");
	const auto filtered = Exchange::PlanImport(
		other,
		registry,
		crafted,
		ExchangeTarget::Sync);
	Require(filtered.error.isEmpty()
		&& filtered.changes.size() == 1
		&& filtered.changes.front().key
			== QString::fromLatin1("nagram.testNumber")
		&& filtered.skippedKeys.size() == 4,
		"a crafted backup must not reach local-only or hidden keys");
	Require(Exchange::PlanImport(other, registry, crafted).changes.size() == 2,
		"a local file may still set local-only keys");

	Require(other.Set(number, 8), "change after preview");
	Require(!Exchange::Apply(other, registry, filtered).applied
		&& other.Get(number) == 8,
		"a stale restore preview must be rejected as a whole");

	Require(other.Set(number, 7), "rollback setup");
	const auto rollback = Exchange::PlanImport(other, registry, before.data);
	Require(rollback.error.isEmpty()
		&& Exchange::Apply(other, registry, rollback).applied
		&& other.Get(number) == 3,
		"importing the earlier local export restores changed values");
}

void TestLocalOnlyList() {
	using namespace Nagram;

	auto registry = Registry();
	Chats::RegisterOptions(registry);
	Chats::RegisterLocalPinOptions(registry);
	Interface::RegisterOptions(registry);
	Compose::RegisterOptions(registry);
	Media::RegisterOptions(registry);
	Media::RegisterLocalFavedOptions(registry);
	Media::RegisterBackendOptions(registry);
	Menu::RegisterOptions(registry);
	Privacy::RegisterOptions(registry);
	Messages::RegisterOptions(registry);
	Filters::RegisterOptions(registry);
	Links::RegisterOptions(registry);
	Links::RegisterBehaviorOptions(registry);
	Links::RegisterInlineOptions(registry);
	Links::RegisterWebviewOptions(registry);
	Network::RegisterOptions(registry);
	Snapshot::RegisterCloudThemeOptions(registry);
	RegisterServiceOptions(registry);
	AutoTranslate::RegisterOptions(registry);
	RegisterDiagnosticsOptions(registry);
	Sync::RegisterOptions(registry);

	const auto expected = QStringList{
		QString::fromLatin1("nagram.appIcon"),
		QString::fromLatin1("nagram.customDoh"),
		QString::fromLatin1("nagram.demoMode"),
		QString::fromLatin1("nagram.disableBackupAddresses"),
		QString::fromLatin1("nagram.downloadSpeedBoost"),
		QString::fromLatin1("nagram.ipStrategy"),
		QString::fromLatin1("nagram.services"),
		QString::fromLatin1("nagram.showRpcErrors"),
		QString::fromLatin1("nagram.uploadSpeedBoost"),
		QString::fromLatin1("nagram.useSystemDns"),
		QString::fromLatin1("nagram.webAppHeightScale"),
		QString::fromLatin1("nagram.webAppWidthScale"),
	};
	Require(Sync::LocalOnlyKeys(registry) == expected,
		"the list of settings kept on this device only changed");
	for (const auto &info : registry.All()) {
		if (info.flags & static_cast<unsigned>(Flag::LocalOnly)) {
			Require(info.scope == Scope::Device
				&& registry.HasFlag(info.key, Flag::Exportable),
				"local-only is meant for exportable device settings");
		}
	}

	auto prefs = MemoryPrefs();
	auto device = Options(prefs);
	Require(device.Set(Network::kIpStrategy, 1)
		&& device.Set(Network::kUseSystemDns, true)
		&& device.Set(Network::kDisableBackupAddresses, true)
		&& device.Set(
			Network::kDownloadSpeedBoost,
			QString::fromLatin1("fast"))
		&& device.Set(Network::kUploadSpeedBoost, true)
		&& device.Set(Privacy::kDemoMode, true)
		&& device.Set(kShowRpcErrors, true)
		&& device.Set(Links::kWebAppWidthScale, 150)
		&& device.Set(Links::kWebAppHeightScale, 150),
		"local-only values");
	const auto sync = Exchange::Export(
		device,
		registry,
		ExchangeTarget::Sync);
	Require(Object(sync.data).value(
			QString::fromLatin1("options")).toObject().isEmpty(),
		"local-only settings reached the backup");
	const auto file = Exchange::Export(device, registry);
	Require(Object(file.data).value(
			QString::fromLatin1("options")).toObject().size() == 9,
		"local-only settings must stay in the local file export");
}

} // namespace

void TestSync() {
	TestCloudThemeRef();
	std::cout << "PASS: Nagram screenshot cloud theme reference" << std::endl;
	TestEnvelope();
	TestDecision();
	TestSupersededBackups();
	TestState();
	TestAllowlist();
	TestLocalOnlyList();
	std::cout << "PASS: Nagram cloud backup" << std::endl;
	TestAutoBackup();
	std::cout << "PASS: Nagram automatic cloud backup" << std::endl;
}
