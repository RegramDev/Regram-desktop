#ifdef _DEBUG

#include "test/test_regram_archive.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "mtproto/mtproto_auth_key.h"
#include "regram/appearance/stories.h"
#include "regram/archive/snapshot.h"
#include "storage/details/storage_file_utilities.h"
#include "test/test_agent.h"
#include "test/test_log.h"

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

namespace Test {
namespace {

using Status = Regram::Archive::Status;

std::shared_ptr<MTP::AuthKey> FictionalKey(int offset = 0) {
	auto data = MTP::AuthKey::Data();
	for (auto i = size_t(); i < data.size(); ++i) {
		data[i] = gsl::byte((i + offset) % 256);
	}
	return std::make_shared<MTP::AuthKey>(data);
}

QByteArray FictionalPayload() {
	return u"Fictional deleted message: 你好 👩‍💻 رسالة"_q.toUtf8();
}

QString Observation(Status status) {
	return u"storage status=%1"_q.arg(int(status));
}

bool WriteEnvelope(
		const QString &path,
		const std::shared_ptr<MTP::AuthKey> &key,
		quint32 version,
		quint32 declaredSize) {
	const auto payload = FictionalPayload();
	auto data = Storage::details::EncryptedDescriptor(
		3 * sizeof(quint32) + sizeof(quint64) + payload.size());
	data.stream << quint32(0x52474131) << version << quint64(101) << declaredSize;
	if (data.stream.writeRawData(payload.constData(), payload.size()) != payload.size()) {
		return false;
	}
	const auto bytes = Storage::details::PrepareEncrypted(data, key);
	auto file = QFile(path);
	return file.open(QIODevice::WriteOnly)
		&& file.write(bytes) == bytes.size();
}

void VerifyBoundaries() {
	auto folder = QTemporaryDir();
	if (!folder.isValid()) {
		Fail(u"Archive fixture directory could not be created"_q);
		return;
	}
	const auto path = folder.filePath(u"retained"_q);
	const auto key = FictionalKey();
	const auto snapshot = Regram::Archive::Snapshot(path, 101, key);
	const auto payload = FictionalPayload();
	const auto written = snapshot.write(payload);
	Check(written == Status::Success, u"Encrypted snapshot write"_q, Observation(written));
	auto file = QFile(path);
	const auto opened = file.open(QIODevice::ReadOnly);
	Check(opened, u"Snapshot file can be inspected"_q);
	const auto cipher = file.readAll();
	file.close();
	Check(!cipher.contains(payload), u"Fictional message is absent from stored bytes"_q,
		u"file bytes=%1, plaintext bytes=%2"_q.arg(cipher.size()).arg(payload.size()));
	const auto foreign = Regram::Archive::Snapshot(path, 102, key);
	const auto foreignRead = foreign.read();
	Check(foreignRead.status == Status::AccountMismatch,
		u"Account identity is checked even with a shared local key"_q,
		Observation(foreignRead.status));
	Check(foreign.write(payload) == Status::AccountMismatch,
		u"Other account cannot replace the snapshot"_q);
	Check(foreign.clear() == Status::AccountMismatch && QFile::exists(path),
		u"Other account cannot clear the snapshot"_q);
	const auto wrongKey = Regram::Archive::Snapshot(path, 101, FictionalKey(13));
	Check(wrongKey.read().status == Status::Corrupt,
		u"Wrong local encryption key cannot read content"_q);
	Check(WriteEnvelope(path, key, 2, payload.size()), u"Future-version fixture written"_q);
	Check(snapshot.read().status == Status::UnsupportedVersion,
		u"Future archive version is reported"_q);
	Check(snapshot.write(payload) == Status::UnsupportedVersion,
		u"Future archive is preserved on write"_q);
	Check(WriteEnvelope(path, key, 1, UINT32_MAX), u"Malformed-length fixture written"_q);
	Check(snapshot.read().status == Status::Corrupt,
		u"Authenticated malformed size is rejected before allocation"_q);
	Check(snapshot.write(payload) == Status::Corrupt,
		u"Corrupt archive is preserved on write"_q);
	Check(snapshot.clear() == Status::Success && !QFile::exists(path),
		u"Explicit clear removes the exact corrupt snapshot"_q);
	const auto tooLarge = QByteArray(8 * 1024 * 1024 + 1, 'x');
	Check(snapshot.write(tooLarge) == Status::TooLarge && !QFile::exists(path),
		u"Oversized snapshot never creates a partial file"_q);
}

void VerifySettingsObservers() {
	using namespace Regram::Appearance;
	SetHideStories(false);
	auto first = std::vector<bool>();
	auto second = std::vector<bool>();
	auto lifetime = rpl::lifetime();
	HideStoriesValue() | rpl::on_next([&](bool value) {
		first.push_back(value);
	}, lifetime);
	HideStoriesValue() | rpl::on_next([&](bool value) {
		second.push_back(value);
	}, lifetime);
	SetHideStories(true);
	const auto expected = std::vector<bool>{ false, true };
	Check(first == expected && second == expected,
		u"Two setting subscribers receive the same changes"_q,
		u"first emissions=%1, second emissions=%2"_q.arg(first.size()).arg(second.size()));
	Check(Core::App().settings().readPref<bool>("regram.appearance.hide-stories"),
		u"Setting has been written to the upstream KV facility"_q);
	Core::App().saveSettings();
}

} // namespace

void VerifyRegramArchive() {
	if (!Active()) {
		return;
	}
	const auto path = EvidenceDir() + u"/restart-retained"_q;
	const auto snapshot = Regram::Archive::Snapshot(path, 101, FictionalKey());
	const auto stage = qEnvironmentVariable("REGRAM_M0_STAGE");
	if (stage == u"write"_q) {
		Check(snapshot.clear() == Status::Success, u"Previous fictional snapshot cleared"_q);
		const auto status = snapshot.write(FictionalPayload());
		Check(status == Status::Success, u"Restart fixture saved"_q, Observation(status));
		VerifySettingsObservers();
		VerifyBoundaries();
	} else if (stage == u"read"_q) {
		const auto result = snapshot.read();
		Check(result.status == Status::Success && result.payload == FictionalPayload(),
			u"Fictional retained content restored after process restart"_q,
			Observation(result.status));
		Check(Regram::Appearance::HideStories(), u"Setting restored after process restart"_q);
		Check(snapshot.clear() == Status::Success && snapshot.read().status == Status::Missing,
			u"Retained content explicitly cleared"_q);
		Regram::Appearance::SetHideStories(false);
		Core::App().saveSettings();
	} else {
		Fail(u"REGRAM_M0_STAGE must be write or read"_q);
	}
}

} // namespace Test

#endif // _DEBUG
