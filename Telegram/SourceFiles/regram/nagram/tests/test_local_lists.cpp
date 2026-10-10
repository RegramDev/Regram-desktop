#include "regram/nagram/chats/local_pins_model.h"
#include "regram/nagram/core/exchange.h"
#include "regram/nagram/media/local_faved_model.h"

#include <QtCore/QJsonArray>

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

void CheckAccountList(
		const Nagram::Registry &registry,
		const Nagram::Option<bool> &toggle,
		const Nagram::Option<QByteArray> &list,
		Nagram::Category category,
		const QByteArray &sample) {
	using namespace Nagram;
	const auto toggleInfo = registry.Find(toggle.key);
	Require(toggleInfo != nullptr
		&& toggleInfo->scope == Scope::Device
		&& toggleInfo->category == category
		&& toggleInfo->fallbackRaw == "0"
		&& registry.HasFlag(toggle.key, Flag::Exportable),
		"local list switch must be an exportable device option, off");
	const auto listInfo = registry.Find(list.key);
	Require(listInfo != nullptr
		&& listInfo->scope == Scope::Account
		&& listInfo->category == category
		&& registry.HasFlag(list.key, Flag::Hidden)
		&& !registry.HasFlag(list.key, Flag::Exportable),
		"local list data must be hidden account data");

	auto devicePrefs = MemoryPrefs();
	auto device = Options(devicePrefs);
	Require(device.Set(toggle, true), "local list switch write");
	const auto exported = Exchange::Export(device, registry);
	const auto values = QJsonDocument::fromJson(exported.data).object().value(
		QString::fromLatin1("options")).toObject();
	Require(values.value(QString::fromUtf8(
			toggle.key.data(), toggle.key.size())) == QJsonValue(true)
		&& !values.contains(QString::fromUtf8(
			list.key.data(), list.key.size())),
		"export must carry the switch and never the local list");

	auto accountPrefs = MemoryPrefs();
	auto account = Options(accountPrefs, Scope::Account);
	Require(account.Get(list).isEmpty(), "local list default");
	Require(account.Set(list, sample) && account.Get(list) == sample,
		"local list write");
	Require(!account.Set(list, QByteArray("{}")),
		"invalid local list accepted on write");
	for (const auto broken : {
			"not json",
			"[]",
			R"({"user":"1","peers":[],"items":[]})",
			R"({"version":2,"user":"1","peers":[],"items":[]})" }) {
		accountPrefs.values[std::string(list.key)] = broken;
		Require(account.Get(list).isEmpty()
			&& account.invalidKeys().contains(list.key)
			&& accountPrefs.values[std::string(list.key)] == broken,
			"broken local list must read empty, be reported and stay stored");
	}
}

void TestLocalPins() {
	using namespace Nagram;
	using namespace Nagram::Chats;
	auto peers = std::vector<quint64>();
	Require(AddLocalPin(peers, 10)
		&& AddLocalPin(peers, 20)
		&& AddLocalPin(peers, 30)
		&& peers == std::vector<quint64>{ 30, 20, 10 },
		"new local pins must go first");
	Require(AddLocalPin(peers, 10)
		&& peers == std::vector<quint64>{ 10, 30, 20 },
		"pinning again must move the chat first without a duplicate");
	Require(!AddLocalPin(peers, 0) && peers.size() == 3,
		"empty peer id accepted");

	const auto raw = SerializeLocalPins(peers, kUser);
	Require(ValidLocalPins(raw)
		&& ParseLocalPins(raw, kUser) == peers,
		"local pins round trip");
	Require(SerializeLocalPins({}, kUser).isEmpty()
		&& ParseLocalPins(QByteArray(), kUser).empty(),
		"empty local pins must clear the stored value");
	const auto large = std::vector<quint64>{ 0xFFFFFFFFFFFFFFFFULL, 1 };
	Require(ParseLocalPins(SerializeLocalPins(large, kUser), kUser) == large,
		"64 bit peer ids must survive the round trip");

	Require(ValidLocalPins(raw) && ParseLocalPins(raw, kOtherUser).empty(),
		"local pins of another user must read as empty");
	auto foreign = ParseLocalPins(raw, kOtherUser);
	Require(AddLocalPin(foreign, 99), "local pin after a user change");
	const auto overwritten = SerializeLocalPins(foreign, kOtherUser);
	Require(ParseLocalPins(overwritten, kOtherUser)
			== std::vector<quint64>{ 99 }
		&& ParseLocalPins(overwritten, kUser).empty(),
		"writing must replace the list of the previous user");

	Require(RemoveLocalPin(peers, 30)
		&& !RemoveLocalPin(peers, 30)
		&& peers == std::vector<quint64>{ 10, 20 },
		"local pin removal");
	peers = { 5, 4, 3, 2, 1 };
	Require(MergeLocalPins(peers, { 4, 1, 77 })
		&& peers == std::vector<quint64>{ 5, 3, 2 },
		"server pins must be removed, the rest keeps its order");
	Require(!MergeLocalPins(peers, { 77 })
		&& peers == std::vector<quint64>{ 5, 3, 2 },
		"merge without common chats changed the list");

	auto full = std::vector<quint64>();
	for (auto i = 1; i <= kLocalPinsLimit; ++i) {
		Require(AddLocalPin(full, quint64(i)), "local pin below the limit");
	}
	const auto before = full;
	Require(!AddLocalPin(full, 1000) && full == before,
		"local pin above the limit must be refused, keeping the list");
	Require(AddLocalPin(full, 1) && int(full.size()) == kLocalPinsLimit,
		"an existing pin must stay movable at the limit");
	Require(ValidLocalPins(SerializeLocalPins(full, kUser)),
		"a full local pin list must stay valid");

	for (const auto broken : {
			"",
			"not json",
			R"({"user":"1","peers":[]})",
			R"({"version":2,"user":"1","peers":[]})",
			R"({"version":"1","user":"1","peers":[]})",
			R"({"version":1,"user":"1","peers":[],"extra":1})",
			R"({"version":1,"user":1,"peers":[]})",
			R"({"version":1,"user":"1","peers":[5]})",
			R"({"version":1,"user":"1","peers":["05"]})",
			R"({"version":1,"user":"1","peers":["-5"]})",
			R"({"version":1,"user":"1","peers":["0"]})",
			R"({"version":1,"user":"1","peers":["5","5"]})",
			R"({"version":1,"user":"1","peers":"5"})" }) {
		Require(!ValidLocalPins(broken)
			&& ParseLocalPins(broken, 1).empty(),
			"broken local pins accepted");
	}
	auto tooMany = full;
	tooMany.push_back(5000);
	Require(!ValidLocalPins(SerializeLocalPins(tooMany, kUser)),
		"local pins above the limit accepted");

	// Local pins must sort between these extremes of the upstream key layouts.
	const auto lowestPinned = 0xFFFFFFFF000000FFULL - 255;
	const auto lastDate = quint64(2145916799); // 2037-12-31 23:59:59 UTC
	const auto highestDate = (lastDate << 32) | 0xFFFFFFFFULL;
	const auto highestUnread = highestDate | 0x8000000000000000ULL;
	const auto highestSorted = 0x8000000000000000ULL
		| (quint64(15) << 59)
		| (lastDate << 28)
		| 0x0FFFFFFFULL;
	for (auto index = 0; index != kLocalPinsLimit; ++index) {
		const auto key = LocalPinSortKeyFor(index);
		Require(key < lowestPinned
			&& key > highestDate
			&& key > highestUnread
			&& key > highestSorted,
			"local pin key must sit between server pins and other chats");
		Require(!index || key < LocalPinSortKeyFor(index - 1),
			"earlier local pins must sort first");
	}

	auto registry = Registry();
	RegisterLocalPinOptions(registry);
	CheckAccountList(registry, kUnlimitedPinnedChats, kLocalPinnedChats,
		Category::Chats, raw);
	Require(registry.HasFlag(kUnlimitedPinnedChats.key,
		Flag::RefreshDialogList), "local pins must refresh the chat list");
}

[[nodiscard]] Nagram::Media::LocalFavedItem FavedItem(quint64 id) {
	return {
		.id = id,
		.set = id + 1000,
		.hash = id * 7,
		.app = 7002010,
		.data = QByteArray::fromHex("00ff10") + QByteArray::number(id),
	};
}

[[nodiscard]] std::vector<quint64> FavedIds(
		const std::vector<Nagram::Media::LocalFavedItem> &items) {
	auto result = std::vector<quint64>();
	for (const auto &item : items) {
		result.push_back(item.id);
	}
	return result;
}

void TestLocalFaved() {
	using namespace Nagram;
	using namespace Nagram::Media;
	auto items = std::vector<LocalFavedItem>();
	Require(AddFavedItem(items, FavedItem(1))
		&& AddFavedItem(items, FavedItem(2))
		&& AddFavedItem(items, FavedItem(3))
		&& FavedIds(items) == std::vector<quint64>{ 3, 2, 1 },
		"new local favorites must go first");
	auto replaced = FavedItem(1);
	replaced.data = "fresh";
	Require(AddFavedItem(items, replaced)
		&& FavedIds(items) == std::vector<quint64>{ 1, 3, 2 }
		&& items.front().data == "fresh",
		"a sticker must be kept once, with its newest data");
	auto withoutSet = FavedItem(9);
	withoutSet.set = 0;
	Require(!AddFavedItem(items, withoutSet) && items.size() == 3,
		"a sticker without a set accepted");

	const auto raw = SerializeLocalFaved(items, kUser);
	auto skipped = 0;
	Require(ValidLocalFaved(raw)
		&& ParseLocalFaved(raw, kUser, &skipped) == items
		&& !skipped, "local favorites round trip");
	auto zeroHash = FavedItem(4);
	zeroHash.hash = 0;
	zeroHash.id = 0xFFFFFFFFFFFFFFFFULL;
	const auto edge = std::vector<LocalFavedItem>{ zeroHash };
	Require(ParseLocalFaved(SerializeLocalFaved(edge, kUser), kUser) == edge,
		"64 bit ids and an empty access hash must survive the round trip");
	Require(SerializeLocalFaved({}, kUser).isEmpty()
		&& ParseLocalFaved(QByteArray(), kUser).empty(),
		"empty local favorites must clear the stored value");

	Require(ParseLocalFaved(raw, kOtherUser).empty(),
		"local favorites of another user must read as empty");
	auto foreign = ParseLocalFaved(raw, kOtherUser);
	Require(AddFavedItem(foreign, FavedItem(50)),
		"local favorite after a user change");
	const auto overwritten = SerializeLocalFaved(foreign, kOtherUser);
	Require(FavedIds(ParseLocalFaved(overwritten, kOtherUser))
			== std::vector<quint64>{ 50 }
		&& ParseLocalFaved(overwritten, kUser).empty(),
		"writing must replace the list of the previous user");

	Require(RemoveFavedItem(items, 3)
		&& !RemoveFavedItem(items, 3)
		&& FavedIds(items) == std::vector<quint64>{ 1, 2 },
		"local favorite removal");
	items = { FavedItem(5), FavedItem(4), FavedItem(3), FavedItem(2) };
	Require(MergeFavedItems(items, { 4, 2, 77 })
		&& FavedIds(items) == std::vector<quint64>{ 5, 3 },
		"server favorites must be removed, the rest keeps its order");
	Require(!MergeFavedItems(items, { 77 }) && items.size() == 2,
		"merge without common stickers changed the list");

	auto full = std::vector<LocalFavedItem>();
	for (auto i = 1; i <= kLocalFavedLimit; ++i) {
		Require(AddFavedItem(full, FavedItem(quint64(i))),
			"local favorite below the limit");
	}
	const auto before = full;
	Require(!AddFavedItem(full, FavedItem(5000)) && full == before,
		"local favorite above the limit must be refused, keeping the list");
	const auto fullRaw = SerializeLocalFaved(full, kUser);
	Require(ValidLocalFaved(fullRaw)
		&& ParseLocalFaved(fullRaw, kUser) == full,
		"a full local favorites list must stay valid");

	auto object = QJsonDocument::fromJson(
		SerializeLocalFaved({ FavedItem(1), FavedItem(2), FavedItem(3) },
			kUser)).object();
	auto list = object.value(QString::fromLatin1("items")).toArray();
	auto damaged = list[1].toObject();
	damaged.insert(QString::fromLatin1("data"), QString::fromLatin1("%%%"));
	list[1] = damaged;
	list.push_back(list[0]);
	list.push_back(QString::fromLatin1("not an object"));
	object.insert(QString::fromLatin1("items"), list);
	const auto partly = QJsonDocument(object).toJson(QJsonDocument::Compact);
	skipped = 0;
	Require(ValidLocalFaved(partly)
		&& FavedIds(ParseLocalFaved(partly, kUser, &skipped))
			== std::vector<quint64>{ 1, 3 }
		&& skipped == 3,
		"a damaged item must be skipped and counted, keeping the rest");

	for (const auto broken : {
			"",
			"not json",
			R"({"user":"1","items":[]})",
			R"({"version":2,"user":"1","items":[]})",
			R"({"version":1,"user":"1","items":[],"extra":1})",
			R"({"version":1,"user":1,"items":[]})",
			R"({"version":1,"user":"01","items":[]})",
			R"({"version":1,"user":"1","items":{}})" }) {
		Require(!ValidLocalFaved(broken)
			&& ParseLocalFaved(broken, 1).empty(),
			"broken local favorites accepted");
	}
	auto tooMany = QJsonArray();
	for (auto i = 0; i <= kLocalFavedLimit; ++i) {
		tooMany.push_back(QJsonObject());
	}
	object.insert(QString::fromLatin1("items"), tooMany);
	Require(!ValidLocalFaved(
		QJsonDocument(object).toJson(QJsonDocument::Compact)),
		"local favorites above the limit accepted");

	auto registry = Registry();
	RegisterLocalFavedOptions(registry);
	CheckAccountList(registry, kUnlimitedFavedStickers, kLocalFavedStickers,
		Category::Media, raw);
}

} // namespace

void TestLocalLists() {
	TestLocalPins();
	TestLocalFaved();
	std::cout << "PASS: Nagram local lists" << std::endl;
}
