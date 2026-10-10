#include "regram/nagram/network/runtime.h"

#include "regram/nagram/network/options.h"
#include "core/application.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "mtproto/mtp_instance.h"
#include "storage/download_manager_mtproto.h"
#include "window/window_controller.h"

#include <QtNetwork/QNetworkReply>

#include <atomic>

namespace Nagram::Network {
namespace {

std::atomic<int> Strategy = 0;
std::atomic<bool> SystemDns = false;

static_assert(kDownloadPart == Storage::kDownloadPartSize);

[[nodiscard]] const QString &DownloadBoost() {
	static const auto result = [] {
		auto &options = ForDevice();
		auto value = options.Get(kDownloadSpeedBoost);
		if (options.invalidKeys().contains(kDownloadSpeedBoost.key)) {
			LOG(("Nagram Network: Bad download acceleration value, "
				"using Telegram defaults."));
		}
		return value;
	}();
	return result;
}

[[nodiscard]] rpl::variable<QString> &DohFailure() {
	static auto result = rpl::variable<QString>();
	return result;
}

void Reconnect() {
	if (!Core::App().domain().started()) {
		return;
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		account->mtp().restart();
	}
}

void EnsureStarted() {
	static auto started = false;
	if (started) {
		return;
	}
	started = true;
	// WHY: session threads can't read Core::Settings, so the main thread
	// mirrors the value here before the first session is configured.
	auto &options = ForDevice();
	Strategy = options.Get(kIpStrategy);
	SystemDns = options.Get(kUseSystemDns);
	auto &failure = DohFailure();
	static auto lifetime = rpl::lifetime();
	options.Value(
		kIpStrategy
	) | rpl::skip(1) | rpl::on_next([](int value) {
		Strategy = value;
		Reconnect();
	}, lifetime);
	options.Value(
		kUseSystemDns
	) | rpl::skip(1) | rpl::on_next([](bool value) {
		SystemDns = value;
		Reconnect();
	}, lifetime);
	options.Value(
		kCustomDoh
	) | rpl::skip(1) | rpl::on_next([&failure] {
		failure = QString();
	}, lifetime);
}

} // namespace

bool UseIPv4(bool upstream) {
	EnsureStarted();
	return ResolveIpChoice(Strategy, { .useIPv4 = upstream }).useIPv4;
}

bool UseIPv6(bool upstream) {
	EnsureStarted();
	return ResolveIpChoice(Strategy, { .useIPv6 = upstream }).useIPv6;
}

QStringList OrderIps(const QStringList &ips) {
	EnsureStarted();
	return OrderIps(Strategy, ips);
}

bool BackupAddressesDisabled() {
	return ForDevice().Get(kDisableBackupAddresses);
}

QString CustomDoh() {
	EnsureStarted();
	return ForDevice().Get(kCustomDoh);
}

void CheckDohReply(
		gsl::not_null<QNetworkReply*> reply,
		const QByteArray &body) {
	if (!SameDohEndpoint(reply->url(), CustomDoh())) {
		return;
	}
	const auto failed = (reply->error() != QNetworkReply::NoError);
	const auto text = failed
		? tr::lng_nagram_custom_doh_failed(
			tr::now,
			lt_error,
			reply->errorString())
		: IsDnsJson(body)
		? QString()
		: tr::lng_nagram_custom_doh_bad_response(tr::now);
	auto &failure = DohFailure();
	if (!text.isEmpty() && failure.current().isEmpty()) {
		LOG(("Nagram Network: Custom DoH request failed, error: %1 (%2)"
			).arg(failed ? reply->errorString() : u"not a JSON answer"_q
			).arg(reply->error()));
		if (const auto window = Core::App().activeWindow()) {
			window->showToast(text);
		}
	}
	failure = text;
}

rpl::producer<QString> CustomDohFailureValue() {
	EnsureStarted();
	return DohFailure().value();
}

int DownloadStartSessions(int upstream) {
	return ResolveDownloadParams(
		DownloadBoost(),
		{ .startSessions = upstream }).startSessions;
}

int DownloadMaxSessions(int upstream) {
	return ResolveDownloadParams(
		DownloadBoost(),
		{ .maxSessions = upstream }).maxSessions;
}

int DownloadStartWindow(int upstream) {
	return ResolveDownloadParams(
		DownloadBoost(),
		{ .startWindow = upstream }).startWindow;
}

int UploadPartSize(qint64 size) {
	static const auto boost = ForDevice().Get(kUploadSpeedBoost);
	return UploadPartSize(boost, size);
}

bool PreferIPv6(bool upstream) {
	return ResolveIpChoice(Strategy, { .preferIPv6 = upstream }).preferIPv6;
}

bool UseSystemDns() {
	return SystemDns;
}

} // namespace Nagram::Network
