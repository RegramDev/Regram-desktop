#include "regram/nagram/network/model.h"

#include "base/basic_types.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <algorithm>

namespace Nagram::Network {
namespace {

[[nodiscard]] bool IsCustomDoh(const QString &data) {
	return data.startsWith(u"https://"_q, Qt::CaseInsensitive);
}

[[nodiscard]] bool IsIPv6(const QString &ip) {
	return !ip.contains(u'.') && ip.contains(u':');
}

} // namespace

IpChoice ResolveIpChoice(int strategy, IpChoice upstream) {
	if (!ValidIpStrategy(strategy)) {
		return upstream;
	}
	switch (static_cast<IpStrategy>(strategy)) {
	case IpStrategy::Follow: return upstream;
	case IpStrategy::Ipv4Only: return { true, false, false };
	case IpStrategy::PreferIpv4: return { true, true, false };
	case IpStrategy::PreferIpv6: return { true, true, true };
	case IpStrategy::Ipv6Only: return { false, true, true };
	}
	return upstream;
}

QStringList OrderIps(int strategy, const QStringList &ips) {
	if (!ValidIpStrategy(strategy)
		|| static_cast<IpStrategy>(strategy) == IpStrategy::Follow) {
		return ips;
	}
	const auto choice = ResolveIpChoice(strategy, {});
	auto v4 = QStringList();
	auto v6 = QStringList();
	for (const auto &ip : ips) {
		if (IsIPv6(ip)) {
			if (choice.useIPv6) {
				v6.push_back(ip);
			}
		} else if (choice.useIPv4) {
			v4.push_back(ip);
		}
	}
	return choice.preferIPv6 ? (v6 + v4) : (v4 + v6);
}

bool ValidDohAddress(const QString &address) {
	if (address.isEmpty()
		|| address.size() > kMaxDohAddressLength
		|| !IsCustomDoh(address)
		|| address.contains(u'@')) {
		return false;
	}
	for (const auto &ch : address) {
		if (ch.isSpace() || !ch.isPrint()) {
			return false;
		}
	}
	const auto url = QUrl(address, QUrl::StrictMode);
	return url.isValid()
		&& !url.host().isEmpty()
		&& url.userInfo().isEmpty()
		&& !url.hasQuery()
		&& !url.hasFragment()
		&& (url.port() == -1 || (url.port() > 0 && url.port() < 65536));
}

bool ValidCustomDoh(const QString &value) {
	return value.isEmpty() || ValidDohAddress(value);
}

void SetDohEndpoint(QUrl &url, const QString &data) {
	if (!IsCustomDoh(data)) {
		url.setHost(data);
		url.setPath(u"/dns-query"_q);
		return;
	}
	const auto custom = QUrl(data, QUrl::StrictMode);
	const auto path = custom.path();
	url.setHost(custom.host());
	url.setPort(custom.port());
	url.setPath((path.isEmpty() || path == u"/"_q)
		? u"/dns-query"_q
		: path);
}

bool SameDohEndpoint(const QUrl &request, const QString &custom) {
	if (!ValidDohAddress(custom)) {
		return false;
	}
	auto url = QUrl();
	url.setScheme(u"https"_q);
	SetDohEndpoint(url, custom);
	return request.scheme() == url.scheme()
		&& !request.host().compare(url.host(), Qt::CaseInsensitive)
		&& request.port(443) == url.port(443)
		&& request.path() == url.path();
}

bool IsDnsJson(const QByteArray &bytes) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(bytes, &error);
	return error.error == QJsonParseError::NoError
		&& document.isObject()
		&& (document.object().contains(u"Status"_q)
			|| document.object().contains(u"Answer"_q));
}

bool ValidDownloadBoost(const QString &value) {
	return value == u"none"_q
		|| value == u"balanced"_q
		|| value == u"fast"_q;
}

DownloadParams ClampDownloadParams(DownloadParams params) {
	const auto max = std::clamp(params.maxSessions, 1, kMaxDownloadSessions);
	return {
		.startSessions = std::clamp(params.startSessions, 1, max),
		.maxSessions = max,
		.startWindow = std::clamp(
			params.startWindow,
			kMinDownloadWindow,
			kMaxDownloadWindow),
	};
}

DownloadParams ResolveDownloadParams(
		const QString &boost,
		DownloadParams upstream) {
	if (boost == u"balanced"_q) {
		return ClampDownloadParams({ 2, 8, 8 * kDownloadPart });
	} else if (boost == u"fast"_q) {
		return ClampDownloadParams({ 4, 12, 16 * kDownloadPart });
	}
	return upstream;
}

int UploadPartSize(bool boost, qint64 size) {
	return (boost && size >= kBoostedUploadFrom) ? kBoostedUploadPart : 0;
}

} // namespace Nagram::Network
