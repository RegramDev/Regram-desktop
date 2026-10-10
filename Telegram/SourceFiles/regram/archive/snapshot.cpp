#include "regram/archive/snapshot.h"

#include "storage/details/storage_file_utilities.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>

namespace Regram::Archive {
namespace {

constexpr auto kMagic = quint32(0x52474131);
constexpr auto kVersion = quint32(1);
constexpr auto kMaxPayloadBytes = 8 * 1024 * 1024;
constexpr auto kMaxEncryptedBytes = kMaxPayloadBytes + 128;

} // namespace

Snapshot::Snapshot(
		QString path,
		std::uint64_t accountId,
		std::shared_ptr<MTP::AuthKey> key)
: _path(std::move(path))
, _accountId(accountId)
, _key(std::move(key)) {
	Expects(!_path.isEmpty() && _accountId && _key != nullptr);
}

ReadResult Snapshot::read() const {
	auto file = QFile(_path);
	if (!file.exists()) {
		return {};
	} else if (!file.open(QIODevice::ReadOnly)) {
		return { .status = Status::IoError };
	} else if (file.size() > kMaxEncryptedBytes) {
		return { .status = Status::TooLarge };
	}
	const auto encrypted = file.readAll();
	if (file.error() != QFileDevice::NoError) {
		return { .status = Status::IoError };
	}
	auto data = Storage::details::EncryptedDescriptor();
	if (!Storage::details::DecryptLocal(data, encrypted, _key)) {
		return { .status = Status::Corrupt };
	}
	auto magic = quint32();
	auto version = quint32();
	auto account = quint64();
	auto size = quint32();
	data.stream >> magic >> version >> account >> size;
	if (data.stream.status() != QDataStream::Ok || magic != kMagic) {
		return { .status = Status::Corrupt };
	} else if (account != _accountId) {
		return { .status = Status::AccountMismatch };
	} else if (version != kVersion) {
		return { .status = Status::UnsupportedVersion };
	} else if (size > kMaxPayloadBytes
		|| size != data.buffer.bytesAvailable()) {
		return { .status = Status::Corrupt };
	}
	auto payload = QByteArray(size, Qt::Uninitialized);
	if (data.stream.readRawData(payload.data(), payload.size()) != payload.size()
		|| data.stream.status() != QDataStream::Ok) {
		return { .status = Status::Corrupt };
	}
	return { .status = Status::Success, .payload = std::move(payload) };
}

Status Snapshot::write(const QByteArray &payload) const {
	if (payload.size() > kMaxPayloadBytes) {
		return Status::TooLarge;
	}
	const auto previous = read();
	if (previous.status != Status::Success && previous.status != Status::Missing) {
		return previous.status;
	}
	auto data = Storage::details::EncryptedDescriptor(
		3 * sizeof(quint32) + sizeof(quint64) + payload.size());
	data.stream << kMagic << kVersion << quint64(_accountId) << quint32(payload.size());
	if (data.stream.writeRawData(payload.constData(), payload.size()) != payload.size()
		|| data.stream.status() != QDataStream::Ok) {
		return Status::IoError;
	}
	const auto encrypted = Storage::details::PrepareEncrypted(data, _key);
	if (!QDir().mkpath(QFileInfo(_path).absolutePath())) {
		return Status::IoError;
	}
	auto file = QSaveFile(_path);
	file.setDirectWriteFallback(false);
	if (!file.open(QIODevice::WriteOnly)) {
		return Status::IoError;
	} else if (file.write(encrypted) != encrypted.size()) {
		file.cancelWriting();
		return Status::IoError;
	}
	return file.commit() ? Status::Success : Status::IoError;
}

Status Snapshot::clear() const {
	const auto result = read();
	if (result.status == Status::Missing) {
		return Status::Success;
	} else if (result.status == Status::AccountMismatch) {
		return result.status;
	}
	return QFile::remove(_path) ? Status::Success : Status::IoError;
}

} // namespace Regram::Archive
