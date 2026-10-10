#pragma once

#include <QtCore/QStringList>
#include <gsl/pointers>
#include <rpl/producer.h>

class QNetworkReply;

namespace Nagram::Network {

// Main thread.
[[nodiscard]] bool UseIPv4(bool upstream);
[[nodiscard]] bool UseIPv6(bool upstream);
[[nodiscard]] QStringList OrderIps(const QStringList &ips);
[[nodiscard]] bool BackupAddressesDisabled();
[[nodiscard]] QString CustomDoh();
void CheckDohReply(
	gsl::not_null<QNetworkReply*> reply,
	const QByteArray &body);
[[nodiscard]] rpl::producer<QString> CustomDohFailureValue();
[[nodiscard]] int DownloadStartSessions(int upstream);
[[nodiscard]] int DownloadMaxSessions(int upstream);
[[nodiscard]] int DownloadStartWindow(int upstream);
[[nodiscard]] int UploadPartSize(qint64 size);

// Thread safe.
[[nodiscard]] bool PreferIPv6(bool upstream);
[[nodiscard]] bool UseSystemDns();

} // namespace Nagram::Network
