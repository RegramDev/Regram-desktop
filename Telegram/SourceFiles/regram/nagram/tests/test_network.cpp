#include "regram/nagram/core/exchange.h"
#include "regram/nagram/network/options.h"
#include "base/basic_types.h"

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
		const Type &changed,
		bool restart = false) {
	using namespace Nagram;
	const auto info = registry.Find(option.key);
	Require(info != nullptr, "network option is not registered");
	Require(info->scope == Scope::Device
		&& info->category == Category::Network
		&& registry.HasFlag(option.key, Flag::Exportable)
		&& registry.HasFlag(option.key, Flag::RequiresRestart) == restart,
		"network option must be an exportable device option");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(options.Get(option) == option.fallback
		&& prefs.values.empty(), "network option default value");
	Require(options.Set(option, changed)
		&& options.Get(option) == changed, "network option write");
	const auto exported = Exchange::Export(options, registry);
	Require(exported.invalidKeys.empty(), "network option export");

	auto importedPrefs = MemoryPrefs();
	auto imported = Options(importedPrefs);
	const auto plan = Exchange::PlanImport(imported, registry, exported.data);
	Require(plan.error.isEmpty() && plan.changes.size() == 1,
		"network option import preview");
	Require(Exchange::Apply(imported, registry, plan).applied
		&& imported.Get(option) == changed, "network option import");

	importedPrefs.values[std::string(option.key)] = "broken";
	Require(imported.Get(option) == option.fallback
		&& imported.invalidKeys().contains(option.key)
		&& importedPrefs.values[std::string(option.key)] == "broken",
		"invalid network value must fall back and stay stored");
}

void TestConnectionOptions() {
	using namespace Nagram;
	using namespace Nagram::Network;
	auto registry = Registry();
	RegisterOptions(registry);
	CheckDeviceOption(registry, kIpStrategy, 4);
	CheckDeviceOption(registry, kDisableBackupAddresses, true);
	Require(kIpStrategy.fallback == 0 && !kDisableBackupAddresses.fallback,
		"connection options must follow Telegram by default");
	for (auto value = 0; value != kIpStrategyCount; ++value) {
		Require(kIpStrategy.validate(value), "IP strategy rejected");
	}
	Require(!kIpStrategy.validate(-1) && !kIpStrategy.validate(5),
		"IP strategy out of range accepted");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Set(kIpStrategy, 5) && prefs.values.empty(),
		"IP strategy out of range stored");
}

void TestIpChoice() {
	using namespace Nagram::Network;
	for (const auto tryIPv6 : { false, true }) {
		for (const auto prefer : { false, true }) {
			const auto upstream = IpChoice{ true, tryIPv6, prefer };
			Require(ResolveIpChoice(0, upstream) == upstream,
				"default IP strategy must return the upstream values");
			Require(ResolveIpChoice(-1, upstream) == upstream
				&& ResolveIpChoice(5, upstream) == upstream,
				"unknown IP strategy must return the upstream values");
			Require(ResolveIpChoice(1, upstream)
				== IpChoice{ true, false, false }, "IPv4 only");
			Require(ResolveIpChoice(2, upstream)
				== IpChoice{ true, true, false }, "prefer IPv4");
			Require(ResolveIpChoice(3, upstream)
				== IpChoice{ true, true, true }, "prefer IPv6");
			const auto only6 = ResolveIpChoice(4, upstream);
			Require(!only6.useIPv4 && only6.useIPv6, "IPv6 only");
		}
	}
}

void TestOrderIps() {
	using namespace Nagram::Network;
	const auto mixed = QStringList{
		u"2001:db8::1"_q,
		u"192.0.2.1"_q,
		u"2001:db8::2"_q,
		u"192.0.2.2"_q,
	};
	const auto v4 = QStringList{ u"192.0.2.1"_q, u"192.0.2.2"_q };
	const auto v6 = QStringList{ u"2001:db8::1"_q, u"2001:db8::2"_q };
	Require(OrderIps(0, mixed) == mixed && OrderIps(9, mixed) == mixed,
		"default IP strategy must keep the resolved list");
	Require(OrderIps(1, mixed) == v4, "IPv4 only list");
	Require(OrderIps(2, mixed) == v4 + v6, "prefer IPv4 list");
	Require(OrderIps(3, mixed) == v6 + v4, "prefer IPv6 list");
	Require(OrderIps(4, mixed) == v6, "IPv6 only list");
	Require(OrderIps(1, v6).isEmpty() && OrderIps(4, v4).isEmpty(),
		"a list without the allowed family must become empty");
	for (auto strategy = 0; strategy != kIpStrategyCount; ++strategy) {
		Require(OrderIps(strategy, {}).isEmpty(), "empty list");
	}
}

void TestDohAddress() {
	using namespace Nagram;
	using namespace Nagram::Network;
	auto registry = Registry();
	RegisterOptions(registry);
	CheckDeviceOption(registry, kUseSystemDns, true);
	CheckDeviceOption(registry, kCustomDoh,
		u"https://dns.example/dns-query"_q);
	Require(!kUseSystemDns.fallback && kCustomDoh.fallback.isEmpty(),
		"domain resolution must follow Telegram by default");

	for (const auto &address : {
			u"https://dns.example"_q,
			u"https://dns.example/"_q,
			u"https://dns.example/dns-query"_q,
			u"https://dns.example:8443/custom/resolve"_q,
			u"HTTPS://DNS.EXAMPLE/dns-query"_q,
			u"https://192.0.2.1/dns-query"_q,
			u"https://[2001:db8::1]:8443/dns-query"_q }) {
		Require(ValidDohAddress(address) && ValidCustomDoh(address),
			"valid DoH address rejected");
	}
	const auto tooLong = u"https://dns.example/"_q
		+ QString(kMaxDohAddressLength, u'a');
	for (const auto &address : {
			QString(),
			u"http://dns.example/dns-query"_q,
			u"dns.example/dns-query"_q,
			u"ftp://dns.example/dns-query"_q,
			u"https://"_q,
			u"https:///dns-query"_q,
			u"https://user@dns.example/dns-query"_q,
			u"https://user:pass@dns.example/dns-query"_q,
			u"https://dns.example/dns-query?token=1"_q,
			u"https://dns.example/dns-query?"_q,
			u"https://dns.example/dns-query#part"_q,
			u"https://dns.example/dns query"_q,
			u"https://dns.example/dns-query\nHost: other"_q,
			u"https://dns.example/\tdns-query"_q,
			u"https://dns.example:0/dns-query"_q,
			u"https://dns.example:99999/dns-query"_q,
			u" https://dns.example/dns-query"_q,
			tooLong }) {
		Require(!ValidDohAddress(address), "invalid DoH address accepted");
	}
	Require(ValidCustomDoh(QString()), "empty custom DoH must be allowed");
	Require(!ValidCustomDoh(u"http://dns.example"_q)
		&& !kCustomDoh.validate(u"http://dns.example"_q),
		"custom DoH option must reject http");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Set(kCustomDoh, u"http://dns.example/dns-query"_q)
		&& !options.Set(kCustomDoh, u"https://a@dns.example/"_q)
		&& prefs.values.empty(), "invalid DoH address stored");
	prefs.values[std::string(kCustomDoh.key)] = "shttp://dns.example";
	Require(options.Get(kCustomDoh).isEmpty()
		&& options.invalidKeys().contains(kCustomDoh.key),
		"stored invalid DoH address must fall back to the built-in services");
}

void TestDohEndpoint() {
	using namespace Nagram::Network;
	const auto make = [](const QString &data) {
		auto url = QUrl();
		url.setScheme(u"https"_q);
		SetDohEndpoint(url, data);
		return url;
	};
	auto upstream = QUrl();
	upstream.setScheme(u"https"_q);
	upstream.setHost(u"mozilla.cloudflare-dns.com"_q);
	upstream.setPath(u"/dns-query"_q);
	Require(make(u"mozilla.cloudflare-dns.com"_q) == upstream,
		"built-in DoH endpoint must match the upstream URL");

	const auto custom = make(u"https://dns.example:8443/custom/resolve"_q);
	Require(custom.host() == u"dns.example"_q
		&& custom.port() == 8443
		&& custom.path() == u"/custom/resolve"_q,
		"custom DoH endpoint must keep host, port and path");
	Require(make(u"https://dns.example"_q).toString()
		== u"https://dns.example/dns-query"_q
		&& make(u"https://dns.example/"_q).toString()
		== u"https://dns.example/dns-query"_q,
		"custom DoH endpoint without a path must use /dns-query");

	auto query = make(u"https://dns.example:8443/custom/resolve"_q);
	query.setQuery(u"name=%1&type=%2&random_padding=%3"_q.arg(
		u"proxy.example"_q).arg(28).arg(u"abc"_q));
	Require(query.toString() == u"https://dns.example:8443/custom/resolve"
		"?name=proxy.example&type=28&random_padding=abc"_q,
		"custom DoH query");

	const auto address = u"https://dns.example/dns-query"_q;
	Require(SameDohEndpoint(make(address), address)
		&& SameDohEndpoint(query, u"https://DNS.example:8443/custom/resolve"_q)
		&& SameDohEndpoint(
			QUrl(u"https://dns.example:443/dns-query?name=a"_q),
			address),
		"custom DoH reply not recognized");
	Require(!SameDohEndpoint(upstream, address)
		&& !SameDohEndpoint(make(address), QString())
		&& !SameDohEndpoint(
			QUrl(u"https://dns.example/resolve"_q),
			address)
		&& !SameDohEndpoint(
			QUrl(u"https://firestore.googleapis.com/v1/projects"_q),
			address),
		"reply from another endpoint treated as custom DoH");

	Require(IsDnsJson("{\"Status\":0,\"Answer\":[]}")
		&& IsDnsJson("{\"Status\":3}")
		&& IsDnsJson("{\"Answer\":[{\"data\":\"192.0.2.1\"}]}"),
		"JSON DNS answer rejected");
	Require(!IsDnsJson(QByteArray())
		&& !IsDnsJson("<html></html>")
		&& !IsDnsJson("[]")
		&& !IsDnsJson("{}")
		&& !IsDnsJson(QByteArray("\x00\x00\x81\x80\x00\x01", 6)),
		"non-JSON DNS answer accepted");
}

void TestTransferOptions() {
	using namespace Nagram;
	using namespace Nagram::Network;
	auto registry = Registry();
	RegisterOptions(registry);
	CheckDeviceOption(registry, kDownloadSpeedBoost, u"fast"_q, true);
	CheckDeviceOption(registry, kUploadSpeedBoost, true, true);
	Require(kDownloadSpeedBoost.fallback == u"none"_q
		&& !kUploadSpeedBoost.fallback,
		"transfer options must follow Telegram by default");
	for (const auto &value : { u"none"_q, u"balanced"_q, u"fast"_q }) {
		Require(ValidDownloadBoost(value), "download level rejected");
	}
	for (const auto &value : {
			QString(),
			u"None"_q,
			u"fast "_q,
			u"extreme"_q,
			u"2"_q }) {
		Require(!ValidDownloadBoost(value), "bad download level accepted");
	}

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Set(kDownloadSpeedBoost, u"extreme"_q)
		&& prefs.values.empty(), "bad download level stored");
	prefs.values[std::string(kDownloadSpeedBoost.key)] = "sextreme";
	Require(options.Get(kDownloadSpeedBoost) == u"none"_q
		&& options.invalidKeys().contains(kDownloadSpeedBoost.key),
		"stored bad download level must be read as none");
}

void TestDownloadParams() {
	using namespace Nagram::Network;
	const auto part = kDownloadPart;
	const auto upstream = DownloadParams{ 1, 8, 4 * part };
	const auto odd = DownloadParams{ 3, 5, 7 * part };
	for (const auto &value : {
			u"none"_q,
			QString(),
			u"extreme"_q,
			u"FAST"_q }) {
		Require(ResolveDownloadParams(value, upstream) == upstream
			&& ResolveDownloadParams(value, odd) == odd
			&& ResolveDownloadParams(value, {}) == DownloadParams(),
			"none and unknown levels must return the upstream values");
	}
	Require(ResolveDownloadParams(u"balanced"_q, upstream)
		== DownloadParams{ 2, 8, 8 * part }, "balanced download level");
	Require(ResolveDownloadParams(u"fast"_q, upstream)
		== DownloadParams{ 4, 12, 16 * part }, "fast download level");
	for (const auto &value : { u"balanced"_q, u"fast"_q }) {
		const auto params = ResolveDownloadParams(value, upstream);
		Require(params == ResolveDownloadParams(value, {})
			&& params == ClampDownloadParams(params),
			"boosted levels must not depend on the upstream values");
		Require(params.startSessions >= 1
			&& params.startSessions <= params.maxSessions
			&& params.maxSessions <= kMaxDownloadSessions
			&& params.startWindow >= kMinDownloadWindow
			&& params.startWindow <= kMaxDownloadWindow
			&& !(params.startWindow % part),
			"download level out of the hard limits");
	}
	Require(kMaxDownloadSessions == 16
		&& kMinDownloadWindow == 4 * part
		&& kMaxDownloadWindow == 16 * part, "download hard limits");
	Require(ClampDownloadParams({ 0, 0, 0 })
		== DownloadParams{ 1, 1, 4 * part }, "clamp from below");
	Require(ClampDownloadParams({ 40, 99, 64 * part })
		== DownloadParams{ 16, 16, 16 * part }, "clamp from above");
	Require(ClampDownloadParams({ 9, 6, 8 * part })
		== DownloadParams{ 6, 6, 8 * part },
		"start sessions must not exceed the session limit");
	Require(ClampDownloadParams({ -3, -1, -part })
		== DownloadParams{ 1, 1, 4 * part }, "clamp negative values");
}

void TestUploadPartSize() {
	using namespace Nagram::Network;
	const auto mb = qint64(1024) * 1024;
	for (const auto size : {
			qint64(0),
			qint64(1),
			mb - 1,
			mb,
			mb + 1,
			32 * mb,
			4000 * mb }) {
		Require(!UploadPartSize(false, size),
			"upload part size must stay with Telegram when turned off");
	}
	Require(!UploadPartSize(true, 0)
		&& !UploadPartSize(true, 1)
		&& !UploadPartSize(true, mb - 1),
		"files under 1 MB must keep the Telegram part size");
	for (const auto size : { mb, mb + 1, 32 * mb, 33 * mb, 4000 * mb }) {
		const auto part = UploadPartSize(true, size);
		Require(part == 512 * 1024
			&& !(part % 1024)
			&& !(524288 % part), "boosted upload part size");
	}
}

} // namespace

void TestNetwork() {
	TestConnectionOptions();
	TestIpChoice();
	TestOrderIps();
	TestDohAddress();
	TestDohEndpoint();
	TestTransferOptions();
	TestDownloadParams();
	TestUploadPartSize();
	std::cout << "PASS: Nagram network options" << std::endl;
}
