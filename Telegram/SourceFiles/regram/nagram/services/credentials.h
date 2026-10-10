#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace Nagram {

enum class CredentialError {
	None,
	Missing,
	Unavailable,
	Denied,
	Invalid,
};

struct CredentialResult {
	QByteArray secret;
	CredentialError error = CredentialError::None;
};

[[nodiscard]] CredentialResult ReadCredential(const QString &account);
[[nodiscard]] CredentialError WriteCredential(
	const QString &account,
	const QByteArray &secret);
[[nodiscard]] CredentialError DeleteCredential(const QString &account);

} // namespace Nagram
