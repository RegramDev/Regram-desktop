#include "regram/nagram/services/auto_translate_model.h"
#include "base/basic_types.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

using namespace Nagram;
using namespace Nagram::AutoTranslate;

class MemoryPrefs final : public RawPrefs {
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

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

constexpr auto kFlag = quint64(0x80) << 48;
constexpr auto kUser = kFlag | quint64(12345);
constexpr auto kChat = kFlag | (quint64(1) << 48) | quint64(777);
constexpr auto kChannel = kFlag | (quint64(2) << 48) | quint64(1234567890);

void TestResolve() {
	const auto modes = { Mode::Inherit, Mode::On, Mode::Off };
	auto checked = 0;
	for (const auto &device : modes) {
		for (const auto &account : modes) {
			for (const auto &chat : modes) {
				const auto expected = (chat != Mode::Inherit)
					? chat
					: (account != Mode::Inherit)
					? account
					: device;
				Require(Resolve(device, account, chat) == expected,
					"resolved mode");
				++checked;
			}
		}
	}
	Require(checked == 27, "all combinations checked");
	Require(Resolve(Mode::On, Mode::On, Mode::Off) == Mode::Off,
		"chat off does not win");
	Require(Resolve(Mode::Off, Mode::Inherit, Mode::On) == Mode::On,
		"chat on does not win");
	Require(Resolve(Mode::On, Mode::Off, Mode::Inherit) == Mode::Off,
		"account does not win over device");
	Require(Resolve(Mode::Inherit, Mode::Inherit, Mode::Inherit)
		== Mode::Inherit, "all inherit");

	for (const auto &enabled : { false, true }) {
		for (const auto &upstream : { false, true }) {
			for (const auto &service : { false, true }) {
				for (const auto &mode : modes) {
					const auto tracking = Tracking(
						enabled, upstream, { mode, service });
					Require(enabled || !tracking,
						"tracking with the upstream switch off");
					if (mode == Mode::Inherit) {
						Require(tracking == (enabled && upstream),
							"inherit differs from upstream");
					} else if (mode == Mode::Off) {
						Require(!tracking, "tracking while off");
					} else if (!service) {
						Require(tracking == (enabled && upstream),
							"on without a service differs from upstream");
					} else {
						Require(tracking == enabled, "on with a service");
					}
				}
			}
		}
	}
	Require(ValidMode(0) && ValidMode(1) && ValidMode(2)
		&& !ValidMode(-1) && !ValidMode(3), "mode range");
}

void TestChats() {
	Require(ValidPeer(kUser) && ValidPeer(kChat) && ValidPeer(kChannel),
		"valid peers rejected");
	Require(!ValidPeer(0) && !ValidPeer(12345) && !ValidPeer(kFlag)
		&& !ValidPeer(kFlag | (quint64(3) << 48) | 1)
		&& !ValidPeer(kFlag | (quint64(0x7F) << 48) | 1)
		&& !ValidPeer(kUser | (quint64(1) << 56)),
		"invalid peers accepted");

	Require(ParseChats({}) && ParseChats({})->empty(), "empty bytes");
	Require(SerializeChats({}).isEmpty(), "empty map is not empty bytes");
	Require(ChatMode({}, kUser) == Mode::Inherit, "default chat mode");

	const auto on = WithChatMode({}, kUser, Mode::On);
	Require(on && ValidChats(*on) && ChatMode(*on, kUser) == Mode::On
		&& ChatMode(*on, kChat) == Mode::Inherit, "chat on");
	const auto both = WithChatMode(*on, kChannel, Mode::Off);
	Require(both && ChatMode(*both, kChannel) == Mode::Off
		&& ChatMode(*both, kUser) == Mode::On, "explicit off stored");
	Require(SerializeChats(*ParseChats(*both)) == *both, "round trip");
	const auto object = QJsonDocument::fromJson(*both).object();
	Require(object.value(u"version"_q) == QJsonValue(1)
		&& object.value(u"chats"_q).toObject().value(
			QString::number(kChannel)) == u"off"_q,
		"stored form");
	const auto removed = WithChatMode(*both, kUser, Mode::Inherit);
	Require(removed && !QJsonDocument::fromJson(*removed).object().value(
			u"chats"_q).toObject().contains(QString::number(kUser))
		&& ChatMode(*removed, kChannel) == Mode::Off,
		"inherit keeps the key");
	const auto cleared = WithChatMode(*removed, kChannel, Mode::Inherit);
	Require(cleared && cleared->isEmpty(), "empty map not cleared");
	Require(!WithChatMode({}, 12345, Mode::On), "invalid peer stored");

	const auto wrap = [](const QByteArray &chats, int version = 1) {
		return QByteArray("{\"version\":") + QByteArray::number(version)
			+ ",\"chats\":" + chats + "}";
	};
	const auto key = QByteArray::number(kUser);
	Require(ValidChats(wrap("{\"" + key + "\":\"on\"}")), "valid map");
	Require(!ValidChats(wrap("{\"" + key + "\":\"on\"}", 2)), "version 2");
	Require(!ValidChats(wrap("{\"" + key + "\":\"inherit\"}")),
		"unknown value");
	Require(!ValidChats(wrap("{\"" + key + "\":true}")), "non-string value");
	Require(!ValidChats(wrap("{\"12345\":\"on\"}")), "invalid peer id");
	Require(!ValidChats(wrap("{\"0" + key + "\":\"on\"}")),
		"non-canonical key");
	Require(!ValidChats(wrap("{\"abc\":\"on\"}")), "non-numeric key");
	Require(!ValidChats(wrap("[]")), "array of chats");
	Require(!ValidChats("{\"version\":1,\"chats\":{},\"extra\":1}"),
		"unknown field");
	Require(!ValidChats("{\"chats\":{}}"), "missing version");
	Require(!ValidChats("[1]") && !ValidChats("{broken"), "broken JSON");
	Require(ChatMode("{broken", kUser) == Mode::Inherit,
		"broken data is not treated as inherit");

	auto many = ChatModes();
	for (auto i = 1; i <= kMaxChats; ++i) {
		many.emplace(kFlag | quint64(i), (i % 2) ? Mode::On : Mode::Off);
	}
	const auto full = SerializeChats(many);
	Require(ValidChats(full) && full.size() <= kMaxChatsBytes,
		"full map rejected");
	Require(!WithChatMode(full, kFlag | quint64(kMaxChats + 1), Mode::On),
		"chat limit exceeded");
	Require(WithChatMode(full, kFlag | quint64(1), Mode::Off).has_value(),
		"existing chat cannot change at the limit");
	many.emplace(kFlag | quint64(kMaxChats + 1), Mode::On);
	Require(!ValidChats(SerializeChats(many)), "oversized map accepted");
	Require(!ValidChats(QByteArray(kMaxChatsBytes + 1, ' ')),
		"oversized bytes accepted");
}

void TestOptions() {
	auto registry = Registry();
	RegisterOptions(registry);
	const auto exportable = [&](std::string_view key) {
		return registry.HasFlag(key, Flag::Exportable);
	};
	Require(registry.All().size() == 3
		&& kDeviceMode.scope == Scope::Device && kDeviceMode.fallback == 0
		&& kAccountMode.scope == Scope::Account && kAccountMode.fallback == 0
		&& kChats.scope == Scope::Account && kChats.fallback.isEmpty()
		&& exportable(kDeviceMode.key)
		&& !exportable(kAccountMode.key)
		&& !exportable(kChats.key),
		"auto-translate option registration");

	auto firstPrefs = MemoryPrefs();
	auto secondPrefs = MemoryPrefs();
	auto first = Options(firstPrefs, Scope::Account);
	auto second = Options(secondPrefs, Scope::Account);
	const auto stored = WithChatMode(first.Get(kChats), kChannel, Mode::On);
	Require(stored && first.Set(kChats, *stored)
		&& first.Set(kAccountMode, int(Mode::Off)), "account write");
	Require(ChatMode(first.Get(kChats), kChannel) == Mode::On
		&& first.Get(kAccountMode) == int(Mode::Off), "first account");
	Require(ChatMode(second.Get(kChats), kChannel) == Mode::Inherit
		&& second.Get(kAccountMode) == int(Mode::Inherit)
		&& secondPrefs.values.empty(), "account values leaked");
	Require(!first.Set(kAccountMode, 5) && !first.Set(kChats, QByteArray("{broken")),
		"invalid account values stored");

	firstPrefs.values[std::string(kChats.key)] = "{broken";
	Require(first.Get(kChats).isEmpty()
		&& first.invalidKeys().contains(kChats.key)
		&& firstPrefs.values[std::string(kChats.key)] == "{broken",
		"unreadable overrides are not kept and reported");
	const auto cleared = WithChatMode(*stored, kChannel, Mode::Inherit);
	Require(cleared && second.Set(kChats, *cleared)
		&& secondPrefs.values.empty(), "inherit writes data");
}

} // namespace

void TestAutoTranslate() {
	TestResolve();
	TestChats();
	TestOptions();
	std::cout << "PASS: Nagram auto-translate modes" << std::endl;
}
