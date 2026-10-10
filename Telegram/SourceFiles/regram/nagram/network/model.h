#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

namespace Nagram::Network {

enum class IpStrategy {
	Follow,
	Ipv4Only,
	PreferIpv4,
	PreferIpv6,
	Ipv6Only,
};

inline constexpr auto kIpStrategyCount = 5;

struct IpChoice {
	bool useIPv4 = true;
	bool useIPv6 = false;
	bool preferIPv6 = false;

	friend bool operator==(const IpChoice &, const IpChoice &) = default;
};

[[nodiscard]] constexpr bool ValidIpStrategy(const int &value) {
	return value >= 0 && value < kIpStrategyCount;
}

[[nodiscard]] IpChoice ResolveIpChoice(int strategy, IpChoice upstream);
[[nodiscard]] QStringList OrderIps(int strategy, const QStringList &ips);

inline constexpr auto kMaxDohAddressLength = 256;

[[nodiscard]] bool ValidDohAddress(const QString &address);
[[nodiscard]] bool ValidCustomDoh(const QString &value);
void SetDohEndpoint(QUrl &url, const QString &data);
[[nodiscard]] bool SameDohEndpoint(const QUrl &request, const QString &custom);
[[nodiscard]] bool IsDnsJson(const QByteArray &bytes);

inline constexpr auto kDownloadPart = 128 * 1024;
inline constexpr auto kMaxDownloadSessions = 16;
inline constexpr auto kMinDownloadWindow = 4 * kDownloadPart;
inline constexpr auto kMaxDownloadWindow = 16 * kDownloadPart;
inline constexpr auto kBoostedUploadPart = 512 * 1024;
inline constexpr auto kBoostedUploadFrom = 1024 * 1024;

struct DownloadParams {
	int startSessions = 0;
	int maxSessions = 0;
	int startWindow = 0;

	friend bool operator==(
		const DownloadParams &,
		const DownloadParams &) = default;
};

[[nodiscard]] bool ValidDownloadBoost(const QString &value);
[[nodiscard]] DownloadParams ClampDownloadParams(DownloadParams params);
[[nodiscard]] DownloadParams ResolveDownloadParams(
	const QString &boost,
	DownloadParams upstream);
[[nodiscard]] int UploadPartSize(bool boost, qint64 size);

} // namespace Nagram::Network
