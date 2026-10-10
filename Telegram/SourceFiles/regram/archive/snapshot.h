#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>
#include <memory>

namespace MTP {
class AuthKey;
} // namespace MTP

namespace Regram::Archive {

enum class Status {
	Success,
	Missing,
	IoError,
	Corrupt,
	UnsupportedVersion,
	AccountMismatch,
	TooLarge,
};

struct ReadResult {
	Status status = Status::Missing;
	QByteArray payload;
};

class Snapshot final {
public:
	Snapshot(
		QString path,
		std::uint64_t accountId,
		std::shared_ptr<MTP::AuthKey> key);

	[[nodiscard]] ReadResult read() const;
	[[nodiscard]] Status write(const QByteArray &payload) const;
	[[nodiscard]] Status clear() const;

private:
	const QString _path;
	const std::uint64_t _accountId = 0;
	const std::shared_ptr<MTP::AuthKey> _key;

};

} // namespace Regram::Archive
