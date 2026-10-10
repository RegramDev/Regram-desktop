#include "regram/nagram/core/exchange.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/media/backend_options.h"
#include "base/basic_types.h"

#include <QtCore/QDir>

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

template <typename Type>
void CheckDeviceOption(
		const Nagram::Registry &registry,
		const Nagram::Option<Type> &option,
		Nagram::Category category,
		const Type &changed) {
	using namespace Nagram;
	const auto info = registry.Find(option.key);
	Require(info != nullptr, "media backend option is not registered");
	Require(info->scope == Scope::Device
		&& info->category == category
		&& registry.HasFlag(option.key, Flag::Exportable)
		&& !registry.HasFlag(option.key, Flag::RequiresRestart),
		"media backend option must be an exportable device option");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(options.Get(option) == option.fallback
		&& prefs.values.empty(), "media backend option default value");
	Require(options.Set(option, changed)
		&& options.Get(option) == changed, "media backend option write");
	const auto exported = Exchange::Export(options, registry);
	Require(exported.invalidKeys.empty(), "media backend option export");

	auto importedPrefs = MemoryPrefs();
	auto imported = Options(importedPrefs);
	const auto plan = Exchange::PlanImport(imported, registry, exported.data);
	Require(plan.error.isEmpty() && plan.changes.size() == 1,
		"media backend option import preview");
	Require(Exchange::Apply(imported, registry, plan).applied
		&& imported.Get(option) == changed, "media backend option import");

	importedPrefs.values[std::string(option.key)] = "broken";
	Require(imported.Get(option) == option.fallback
		&& imported.invalidKeys().contains(option.key)
		&& importedPrefs.values[std::string(option.key)] == "broken",
		"invalid media backend value must fall back and stay stored");
}

void TestVoiceBitrate() {
	using namespace Nagram;
	using namespace Nagram::Media;
	auto registry = Registry();
	RegisterBackendOptions(registry);
	CheckDeviceOption(registry, kVoiceRecordBitrate, Category::Media, 64);
	Require(kVoiceRecordBitrate.fallback == 0,
		"voice bitrate must follow Telegram by default");
	for (const auto value : { 0, 16, 24, 48, 64, 96, 128 }) {
		Require(kVoiceRecordBitrate.validate(value),
			"voice bitrate rejected");
	}
	for (const auto value : { 32, 100, -16, 8, 129, 16000 }) {
		Require(!kVoiceRecordBitrate.validate(value),
			"unsupported voice bitrate accepted");
	}

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(VoiceRecordBitrate(options, 32000) == 32000
		&& VoiceRecordBitrate(options, 12345) == 12345,
		"default voice bitrate must return the upstream value");
	Require(!options.Set(kVoiceRecordBitrate, 32)
		&& !options.Set(kVoiceRecordBitrate, 100)
		&& !options.Set(kVoiceRecordBitrate, -16)
		&& prefs.values.empty(), "unsupported voice bitrate stored");
	for (const auto value : { 16, 24, 48, 64, 96, 128 }) {
		Require(options.Set(kVoiceRecordBitrate, value)
			&& VoiceRecordBitrate(options, 32000) == value * 1000,
			"selected voice bitrate");
	}
	Require(options.Set(kVoiceRecordBitrate, 0)
		&& prefs.values.empty()
		&& VoiceRecordBitrate(options, 32000) == 32000,
		"voice bitrate reset");

	auto errors = 0;
	auto lifetime = rpl::lifetime();
	options.readErrors() | rpl::on_next([&](std::string_view key) {
		errors += (key == kVoiceRecordBitrate.key) ? 1 : 0;
	}, lifetime);
	prefs.values[std::string(kVoiceRecordBitrate.key)] = "100";
	Require(VoiceRecordBitrate(options, 32000) == 32000 && errors == 1,
		"stored unsupported bitrate must fall back and be reported");
	Require(ResolveVoiceBitrate(100, 32000) == 32000
		&& ResolveVoiceBitrate(-1, 32000) == 32000,
		"unsupported bitrate must never reach the encoder");
}

void TestGroupCallRawAudio() {
	using namespace Nagram;
	using namespace Nagram::Media;
	auto registry = Registry();
	RegisterBackendOptions(registry);
	CheckDeviceOption(registry, kGroupCallRawAudio, Category::Media, true);
	Require(!kGroupCallRawAudio.fallback,
		"group call audio processing must stay on by default");
}

void TestCoverAddress() {
	using namespace Nagram;
	using namespace Nagram::Media;
	for (const auto &address : {
			u"https://covers.example/art?artist={artist}&title={title}"_q,
			u"https://covers.example/{title}.jpg"_q,
			u"https://covers.example/{title}/{title}"_q,
			u"https://covers.example:8443/search?q="_q,
			u"https://covers.example/search/"_q,
			u"http://localhost:8080/cover?q={title}"_q,
			u"http://127.0.0.1:8080/cover?q="_q,
			u"http://127.10.20.30/cover?q="_q,
			u"http://[::1]:8080/cover?q={title}"_q }) {
		Require(ValidCoverAddress(address) && ValidCoverUrl(address),
			"valid cover address rejected");
	}
	const auto tooLong = u"https://covers.example/"_q
		+ QString(kMaxCoverUrlLength, u'a');
	for (const auto &address : {
			QString(),
			u"http://covers.example/cover?q={title}"_q,
			u"http://127.evil.example/cover?q="_q,
			u"http://127.0.0.1.evil.example/cover?q="_q,
			u"http://1270.0.0.1/cover?q="_q,
			u"file:///etc/passwd"_q,
			u"file://localhost/c:/cover?q={title}"_q,
			u"ftp://covers.example/{title}"_q,
			u"covers.example/cover?q={title}"_q,
			u"https://"_q,
			u"https:///cover?q={title}"_q,
			u"https://user:pass@covers.example/{title}"_q,
			u"https://covers.example/{title}#part"_q,
			u"https://covers.example/cover\nHost: other?q={title}"_q,
			u"https://covers.example/co ver?q={title}"_q,
			u"https://covers.example/\tcover?q="_q,
			u"https://covers.example/cover?a={artist}"_q,
			u"https://covers.example/cover?q={title}&x={album}"_q,
			u"https://covers.example/cover?q={title"_q,
			u"https://covers.example/cover?q=title}"_q,
			u"https://covers.example/cover?q={}"_q,
			u"https://{title}.example/cover"_q,
			u"https://covers.example{title}"_q,
			u"https://covers.example:0/{title}"_q,
			u"https://covers.example:99999/{title}"_q,
			u" https://covers.example/{title}"_q,
			tooLong }) {
		Require(!ValidCoverAddress(address), "invalid cover address accepted");
	}
	Require(ValidCoverUrl(QString()), "empty cover address must be allowed");
	Require(CoverUrlHost(u"https://Covers.Example:8443/{title}"_q)
		== u"covers.example"_q
		&& CoverUrlHost(u"file:///etc/passwd"_q).isEmpty(),
		"cover address host");
}

void TestCoverExpansion() {
	using namespace Nagram::Media;
	const auto artist = QString::fromUtf8("AC/DC & Friends #1");
	const auto title = QString::fromUtf8("周杰伦 🎵 a+b?");
	const auto encodedArtist = u"AC%2FDC%20%26%20Friends%20%231"_q;
	const auto encodedTitle = u"%E5%91%A8%E6%9D%B0%E4%BC%A6%20"
		"%F0%9F%8E%B5%20a%2Bb%3F"_q;
	Require(ExpandCoverUrl(
		u"https://covers.example/art?artist={artist}&title={title}"_q,
		artist,
		title) == u"https://covers.example/art?artist="_q + encodedArtist
			+ u"&title="_q + encodedTitle,
		"cover template expansion");
	Require(ExpandCoverUrl(
		u"https://covers.example/{title}/{title}.jpg"_q,
		artist,
		u"a b"_q) == u"https://covers.example/a%20b/a%20b.jpg"_q,
		"repeated cover placeholder");
	Require(ExpandCoverUrl(
		u"https://covers.example/?a={artist}&t={title}"_q,
		u"{title}"_q,
		u"{artist}"_q)
		== u"https://covers.example/?a=%7Btitle%7D&t=%7Bartist%7D"_q,
		"placeholder text inside a value must not expand again");
	Require(ExpandCoverUrl(
		u"https://covers.example/search?q="_q,
		u"Artist"_q,
		u"Song Name"_q)
		== u"https://covers.example/search?q=Artist%20-%20Song%20Name"_q
		&& ExpandCoverUrl(
			u"https://covers.example/search?q="_q,
			QString(),
			u"Song"_q) == u"https://covers.example/search?q=Song"_q,
		"address without placeholders must get the query appended");
	Require(ExpandCoverUrl(u"file:///cover?q="_q, artist, title).isEmpty()
		&& ExpandCoverUrl(QString(), artist, title).isEmpty(),
		"invalid cover address must not produce a request");
}

void TestCoverOption() {
	using namespace Nagram;
	using namespace Nagram::Media;
	auto registry = Registry();
	RegisterBackendOptions(registry);
	const auto info = registry.Find(kMusicCoverUrl.key);
	Require(info != nullptr
		&& info->scope == Scope::Device
		&& info->category == Category::Media
		&& info->type == OptionInfo::ValueType::String
		&& registry.HasFlag(kMusicCoverUrl.key, Flag::Hidden)
		&& !registry.HasFlag(kMusicCoverUrl.key, Flag::Exportable)
		&& !registry.HasFlag(kMusicCoverUrl.key, Flag::RequiresRestart),
		"cover address must be a hidden device option");
	Require(kMusicCoverUrl.fallback.isEmpty(),
		"covers must come from Telegram by default");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(CoverRequestUrl(options, u"Artist"_q, u"Song"_q).isEmpty()
		&& prefs.values.empty(),
		"default cover address must keep the upstream location");
	Require(!options.Set(kMusicCoverUrl, u"http://covers.example/{title}"_q)
		&& !options.Set(kMusicCoverUrl, u"file:///cover?q={title}"_q)
		&& !options.Set(kMusicCoverUrl, u"https://covers.example/{album}"_q)
		&& prefs.values.empty(), "invalid cover address stored");
	const auto address = u"https://covers.example/?token=secret&q={title}"_q;
	Require(options.Set(kMusicCoverUrl, address)
		&& CoverRequestUrl(options, u"Artist"_q, u"Song"_q)
			== u"https://covers.example/?token=secret&q=Song"_q,
		"cover address write");
	const auto exported = Exchange::Export(options, registry);
	Require(!exported.data.contains("musicCoverUrl")
		&& !exported.data.contains("secret"),
		"cover address must not be exported");

	auto errors = 0;
	auto lifetime = rpl::lifetime();
	options.readErrors() | rpl::on_next([&](std::string_view key) {
		errors += (key == kMusicCoverUrl.key) ? 1 : 0;
	}, lifetime);
	prefs.values[std::string(kMusicCoverUrl.key)]
		= "shttp://covers.example/{title}";
	Require(CoverRequestUrl(options, u"Artist"_q, u"Song"_q).isEmpty()
		&& errors == 1
		&& options.invalidKeys().contains(kMusicCoverUrl.key),
		"stored invalid cover address must fall back and be reported");
}

void TestExportOptions() {
	using namespace Nagram;
	using namespace Nagram::Media;
	auto registry = Registry();
	RegisterBackendOptions(registry);
	CheckDeviceOption(registry, kStickerExportAutoSync, Category::Media, true);
	CheckDeviceOption(registry, kStickerExportDirNaming, Category::Media, 2);
	Require(!kStickerExportAutoSync.fallback
		&& kStickerExportDirNaming.fallback == 0
		&& kStickerExportPath.fallback.isEmpty(),
		"sticker export must be off by default");
	Require(kStickerExportDirNaming.validate(0)
		&& kStickerExportDirNaming.validate(1)
		&& kStickerExportDirNaming.validate(2)
		&& !kStickerExportDirNaming.validate(3)
		&& !kStickerExportDirNaming.validate(-1),
		"export folder naming range");
	Require(registry.HasFlag(kStickerExportPath.key, Flag::Hidden)
		&& !registry.HasFlag(kStickerExportPath.key, Flag::Exportable)
		&& registry.Find(kStickerExportPath.key)->scope == Scope::Device,
		"export folder must be a hidden device option");

	const auto path = QDir(QDir::tempPath()).absoluteFilePath(u"stickers"_q);
	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(ValidExportPath(QString()) && ValidExportPath(path),
		"export folder rejected");
	Require(!ValidExportPath(u"relative/folder"_q)
		&& !ValidExportPath(u"stickers"_q)
		&& !ValidExportPath(path + QString(kMaxExportPathLength, u'a'))
		&& !ValidExportPath(path + u"\nnext"_q),
		"invalid export folder accepted");
	Require(!options.Set(kStickerExportPath, u"relative/folder"_q)
		&& prefs.values.empty(), "relative export folder stored");
	Require(options.Set(kStickerExportPath, path)
		&& options.Get(kStickerExportPath) == path, "export folder write");
	Require(!Exchange::Export(options, registry).data.contains("stickers"),
		"export folder must not be exported");
}

void TestExportDirNames() {
	using namespace Nagram::Media;
	Require(SanitizeExportDirName(u"Cats/and\\dogs"_q) == u"Catsanddogs"_q
		&& SanitizeExportDirName(u"a<b>c:d\"e|f?g*h"_q) == u"abcdefgh"_q
		&& SanitizeExportDirName(u"tab\there\nnow"_q) == u"tabherenow"_q
		&& SanitizeExportDirName(u"  name. . "_q) == u"name"_q
		&& SanitizeExportDirName(u".."_q).isEmpty()
		&& SanitizeExportDirName(u"///"_q).isEmpty()
		&& SanitizeExportDirName(QString::fromUtf8("猫咪 贴纸"))
			== QString::fromUtf8("猫咪 贴纸"),
		"folder name cleanup");
	const auto longTitle = QString(200, u'a');
	Require(SanitizeExportDirName(longTitle).size() == kMaxExportDirLength,
		"folder name length limit");
	const auto emoji = QString(kMaxExportDirLength - 1, u'a')
		+ QString::fromUtf8("😀");
	const auto cut = SanitizeExportDirName(emoji);
	Require(cut.size() == kMaxExportDirLength - 1
		&& !cut.back().isHighSurrogate(),
		"folder name must not end with half of a character");

	Require(SafeExportDirName(u"cats"_q)
		&& SafeExportDirName(u"_42"_q)
		&& SafeExportDirName(u"CON_42"_q)
		&& !SafeExportDirName(QString())
		&& !SafeExportDirName(u".."_q)
		&& !SafeExportDirName(u"."_q)
		&& !SafeExportDirName(u"a/b"_q)
		&& !SafeExportDirName(u"../cats"_q)
		&& !SafeExportDirName(u"/cats"_q)
		&& !SafeExportDirName(u"C:\\cats"_q)
		&& !SafeExportDirName(u"cats."_q)
		&& !SafeExportDirName(u"cats "_q)
		&& !SafeExportDirName(u"NUL"_q)
		&& !SafeExportDirName(u"com1.txt"_q),
		"safe folder name");

	const auto sets = std::vector<ExportSetInfo>{
		{ .id = 11, .shortName = u"cats"_q, .title = u"Funny / Cats"_q },
		{ .id = 12, .shortName = u"CON"_q, .title = u"NUL.txt"_q },
		{ .id = 13, .shortName = QString(), .title = u"..."_q },
		{ .id = 14, .shortName = u"Dogs"_q, .title = u"Same"_q },
		{ .id = 15, .shortName = u"dogs"_q, .title = u"Same"_q },
	};
	Require(ExportDirNames(sets, 0) == std::vector<QString>{
		u"cats"_q, u"CON_12"_q, u"_13"_q, u"Dogs_14"_q, u"dogs_15"_q },
		"folder names from short names");
	Require(ExportDirNames(sets, 1) == std::vector<QString>{
		u"Funny  Cats"_q,
		u"NUL_txt_12"_q,
		u"_13"_q,
		u"Same_14"_q,
		u"Same_15"_q }, "folder names from titles");
	Require(ExportDirNames(sets, 2) == std::vector<QString>{
		u"11"_q, u"12"_q, u"13"_q, u"14"_q, u"15"_q },
		"folder names from ids");
	auto reversed = sets;
	std::reverse(reversed.begin(), reversed.end());
	auto names = ExportDirNames(reversed, 1);
	std::reverse(names.begin(), names.end());
	Require(names == ExportDirNames(sets, 1),
		"folder names must not depend on the order of sets");
	for (const auto &name : ExportDirNames(sets, 1)) {
		Require(SafeExportDirName(name), "generated folder name is unsafe");
	}
	const auto chained = std::vector<ExportSetInfo>{
		{ .id = 5, .shortName = u"x"_q },
		{ .id = 6, .shortName = u"X"_q },
		{ .id = 7, .shortName = u"x_5"_q },
	};
	const auto unique = ExportDirNames(chained, 0);
	Require(unique[0].toCaseFolded() != unique[1].toCaseFolded()
		&& unique[0].toCaseFolded() != unique[2].toCaseFolded()
		&& unique[1].toCaseFolded() != unique[2].toCaseFolded(),
		"folder names must stay unique after adding ids");

	Require(ExportFileName(42, u"webp"_q) == u"42.webp"_q
		&& SafeExportFileName(u"42.webp"_q)
		&& SafeExportFileName(u"18446744073709551615.tgs"_q)
		&& SafeExportFileName(u"7.webm"_q)
		&& !SafeExportFileName(u"42.exe"_q)
		&& !SafeExportFileName(u"../42.webp"_q)
		&& !SafeExportFileName(u"042.webp"_q)
		&& !SafeExportFileName(u"42.webp.tgs"_q)
		&& !SafeExportFileName(u"set.json"_q)
		&& !SafeExportFileName(u".webp"_q),
		"sticker file names");
}

void TestExportManifests() {
	using namespace Nagram::Media;
	const auto manifest = ExportManifest{ {
		{ 18446744073709551615ULL, u"cats"_q, 123456789012345678ULL, 30 },
		{ 2, QString::fromUtf8("猫咪"), 0, 0 },
	} };
	const auto bytes = SerializeExportManifest(manifest);
	Require(ParseExportManifest(bytes) == manifest, "root manifest round trip");
	Require(ParseExportManifest(R"({"version":1,"sets":[]})")
		== ExportManifest(), "empty root manifest");
	const auto copy = bytes;
	for (const auto &broken : {
			QByteArray(),
			QByteArray("not json"),
			QByteArray("[]"),
			QByteArray(R"({"version":2,"sets":[]})"),
			QByteArray(R"({"version":"1","sets":[]})"),
			QByteArray(R"({"version":1})"),
			QByteArray(R"({"version":1,"sets":{}})"),
			QByteArray(R"({"version":1,"sets":[],"extra":true})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0"}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":1,"dir":"a","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":"1"}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":1.5}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":-1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":1,"x":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"0","dir":"a","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"..","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"../a","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"/abs","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a\\b","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":1},{"id":"1","dir":"b","hash":"0","count":1}]})"),
			QByteArray(R"({"version":1,"sets":[{"id":"1","dir":"a","hash":"0","count":1},{"id":"2","dir":"A","hash":"0","count":1}]})") }) {
		Require(!ParseExportManifest(broken), "broken root manifest accepted");
	}
	Require(bytes == copy, "parsing must not change the manifest bytes");

	const auto set = ExportSetManifest{
		.id = 42,
		.shortName = u"cats"_q,
		.title = QString::fromUtf8("Cats \"quoted\" 猫"),
		.stickers = {
			{ 3, u"3.webp"_q, QString::fromUtf8("😀") },
			{ 1, u"1.tgs"_q, QString() },
			{ 2, u"2.webm"_q, QString::fromUtf8("🐱") },
		},
	};
	Require(ParseExportSetManifest(SerializeExportSetManifest(set)) == set,
		"set manifest round trip keeps the sticker order");
	for (const auto &broken : {
			QByteArray(),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b"})"),
			QByteArray(R"({"version":3,"id":"1","shortName":"a","title":"b","stickers":[]})"),
			QByteArray(R"({"version":1,"id":1,"shortName":"a","title":"b","stickers":[]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":7,"title":"b","stickers":[]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[],"x":0})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"1.webp"}]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"../1.webp","emoji":""}]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"/etc/passwd","emoji":""}]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"2.webp","emoji":""}]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"1.exe","emoji":""}]})"),
			QByteArray(R"({"version":1,"id":"1","shortName":"a","title":"b","stickers":[{"id":"1","file":"1.webp","emoji":""},{"id":"1","file":"1.webp","emoji":""}]})") }) {
		Require(!ParseExportSetManifest(broken),
			"broken set manifest accepted");
	}
}

void TestExportPlan() {
	using namespace Nagram::Media;
	const auto current = std::vector<ExportSetInfo>{
		{ 1, 100, 10, u"cats"_q, u"Cats"_q },
		{ 2, 200, 20, u"dogs"_q, u"Dogs"_q },
		{ 3, 300, 30, u"birds"_q, u"Birds"_q },
	};
	const auto same = ExportManifest{ {
		{ 1, u"cats"_q, 100, 10 },
		{ 2, u"dogs"_q, 200, 20 },
		{ 3, u"birds"_q, 300, 30 },
	} };
	Require(PlanExport(same, current, 0).empty(),
		"unchanged sets must give an empty plan");

	const auto fresh = PlanExport(ExportManifest(), current, 0);
	const auto freshSets = std::vector<ExportPlanSet>{
		{ 1, u"cats"_q, QString(), true },
		{ 2, u"dogs"_q, QString(), true },
		{ 3, u"birds"_q, QString(), true },
	};
	Require(fresh.removed.empty() && fresh.sets == freshSets,
		"first export writes every set");

	const auto changed = ExportManifest{ {
		{ 1, u"cats"_q, 101, 10 },
		{ 2, u"dogs"_q, 200, 19 },
		{ 3, u"birds"_q, 300, 30 },
		{ 4, u"fish"_q, 400, 40 },
	} };
	const auto update = PlanExport(changed, current, 0);
	const auto updateSets = std::vector<ExportPlanSet>{
		{ 1, u"cats"_q, QString(), true },
		{ 2, u"dogs"_q, QString(), true },
	};
	const auto updateRemoved = std::vector<quint64>{ 4 };
	Require(update.sets == updateSets && update.removed == updateRemoved,
		"changed hash or count rewrites a set, a removed set is dropped");

	const auto renamed = PlanExport(same, current, 1);
	const auto renamedSets = std::vector<ExportPlanSet>{
		{ 1, u"Cats"_q, u"cats"_q, false },
		{ 2, u"Dogs"_q, u"dogs"_q, false },
		{ 3, u"Birds"_q, u"birds"_q, false },
	};
	Require(renamed.removed.empty() && renamed.sets == renamedSets,
		"a new naming renames folders without rewriting them");
	Require(PlanExport(same, current, 9).empty(),
		"unknown naming must fall back to short names");

	const auto corrupt = PlanExport(std::nullopt, current, 0);
	Require(corrupt.removed.empty()
		&& corrupt.sets.size() == 3
		&& corrupt.sets[0].write
		&& corrupt.sets[0].renameFrom.isEmpty(),
		"a broken manifest exports everything and removes nothing");

	const auto before = ExportSetManifest{
		.id = 1,
		.stickers = {
			{ 1, u"1.webp"_q, QString() },
			{ 2, u"2.webp"_q, QString() },
			{ 3, u"3.tgs"_q, QString() },
		},
	};
	const auto after = ExportSetManifest{
		.id = 1,
		.stickers = {
			{ 3, u"3.tgs"_q, QString() },
			{ 1, u"1.webp"_q, QString() },
			{ 4, u"4.webm"_q, QString() },
		},
	};
	Require(StaleExportFiles(before, after) == QStringList{ u"2.webp"_q },
		"only stickers of the previous manifest are removed");
	auto other = before;
	other.id = 9;
	Require(StaleExportFiles(std::nullopt, after).isEmpty()
		&& StaleExportFiles(other, after).isEmpty(),
		"nothing is removed without a matching previous manifest");

	auto manifest = same;
	UpsertExportSet(manifest, { 2, u"dogs_2"_q, 201, 21 });
	UpsertExportSet(manifest, { 5, u"fish"_q, 500, 5 });
	RemoveExportSets(manifest, { 1, 99 });
	Require(manifest == ExportManifest{ {
		{ 2, u"dogs_2"_q, 201, 21 },
		{ 3, u"birds"_q, 300, 30 },
		{ 5, u"fish"_q, 500, 5 } } }, "manifest update");
}

void TestExportRun() {
	using namespace Nagram::Media;
	auto run = ExportRun();
	Require(!run.status().running && !run.autoSyncPaused(), "idle run");

	const auto first = run.begin(false);
	Require(run.plan(first, { 1, 2, 3 })
		&& run.status().running
		&& run.status().total == 3, "run start");
	Require(run.setDone(first, 1) && run.status().done == 1, "set done");
	Require(!run.setDone(first, 1)
		&& !run.setFailed(first, 1)
		&& run.status().done == 1,
		"a set finished twice must be counted once");
	Require(!run.setDone(first, 9), "unknown set counted");

	const auto second = run.begin(false);
	Require(second != first
		&& !run.accepts(first)
		&& !run.setDone(first, 2)
		&& !run.setFailed(first, 3)
		&& !run.fail(first, ExportError::Write)
		&& run.status().done == 0
		&& !run.autoSyncPaused(),
		"callbacks of an older round must be rejected");
	Require(run.plan(second, { 1, 2 }), "second run plan");

	run.invalidate();
	Require(!run.accepts(second)
		&& !run.setDone(second, 1)
		&& !run.status().running
		&& !run.status().finished,
		"a changed folder must invalidate the running round");

	const auto third = run.begin(false);
	Require(run.plan(third, { 1, 2 })
		&& run.setFailed(third, 1)
		&& run.setDone(third, 2)
		&& !run.status().running
		&& run.status().finished
		&& run.status().failed == 1
		&& run.status().error == ExportError::None
		&& !run.autoSyncPaused(),
		"a failed download ends the round without pausing");

	const auto fourth = run.begin(false);
	Require(run.plan(fourth, { 1, 2 })
		&& run.setDone(fourth, 1)
		&& run.fail(fourth, ExportError::Write)
		&& !run.status().running
		&& run.status().error == ExportError::Write
		&& run.autoSyncPaused()
		&& !run.setDone(fourth, 2),
		"a write failure stops the round and pauses automatic sync");

	const auto fifth = run.begin(false);
	Require(run.plan(fifth, {})
		&& run.status().finished
		&& run.autoSyncPaused(),
		"an automatic round must not lift the pause");
	const auto sixth = run.begin(true);
	Require(run.fail(sixth, ExportError::Path)
		&& run.status().error == ExportError::Path
		&& run.autoSyncPaused(),
		"a failed manual round keeps the pause");
	const auto seventh = run.begin(true);
	Require(run.plan(seventh, { 1 })
		&& run.autoSyncPaused()
		&& run.setDone(seventh, 1)
		&& run.status().finished
		&& !run.autoSyncPaused(),
		"a successful manual round resumes automatic sync");
}

void TestWebAppPlatform() {
	using namespace Nagram;
	using namespace Nagram::Links;
	auto registry = Registry();
	RegisterBehaviorOptions(registry);
	CheckDeviceOption(registry, kWebAppAndroidPlatform, Category::Rules, true);
	Require(!kWebAppAndroidPlatform.fallback,
		"web apps must get the upstream platform by default");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	const auto platform = [&] {
		return std::string(
			WebAppPlatformName(options.Get(kWebAppAndroidPlatform)));
	};
	Require(platform() == "tdesktop", "default web app platform");
	Require(options.Set(kWebAppAndroidPlatform, true)
		&& platform() == "android", "Android web app platform");
	Require(options.Set(kWebAppAndroidPlatform, false)
		&& prefs.values.empty()
		&& platform() == "tdesktop", "web app platform reset");
	prefs.values[std::string(kWebAppAndroidPlatform.key)] = "broken";
	Require(platform() == "tdesktop",
		"stored invalid value must keep the upstream platform");
}

} // namespace

void TestMedia() {
	TestVoiceBitrate();
	TestGroupCallRawAudio();
	TestCoverAddress();
	TestCoverExpansion();
	TestCoverOption();
	TestExportOptions();
	TestExportDirNames();
	TestExportManifests();
	TestExportPlan();
	TestExportRun();
	TestWebAppPlatform();
	std::cout << "PASS: Nagram media backends" << std::endl;
}
