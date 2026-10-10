#ifdef _DEBUG

#include "test/test_regram_integration.h"

#include "test/test_agent.h"
#include "test/test_log.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "regram/ayu/ayu_settings.h"
#include "regram/ayu/data/ayu_database.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/messages/options.h"

#include "core/version.h"
#include "lang/lang_instance.h"
#include "lang_auto_counts.h"
#include "regram/nagram/core/language.h"
#include "regram/nagram/sync/model.h"
#include "regram/nagram/interface/app_icon.h"
#include "regram/ayu/ui/ayu_logo.h"

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QFile>

namespace Test {
namespace {

constexpr auto kSettingsKey = "regram.advanced.settings-v1";
constexpr auto kFirstUser = ID(-101);
constexpr auto kSecondUser = ID(-102);
constexpr auto kDialog = ID(501);

bool DisposableProfile() {
	if (!Active() || !QFile::exists(cWorkingDir() + u"testing"_q)) {
		return false;
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (account->maybeSession()) {
			return false;
		}
	}
	return true;
}

void VerifySettings() {
	auto &core = Core::App().settings();
	core.writePref<QByteArray>(kSettingsKey, R"({
		"version":1,"futureEnvelope":"keep",
		"settings":{"disableStories":true,"future":"preserve",
		"ghostModeSettings":{"0":{"sendReadMessages":true,"futureGhost":"keep"}}}
	})");
	AyuSettings::load();
	Check(AyuSettings::StorageAvailable(), u"Advanced settings load"_q);
	AyuSettings::getInstance().setShowMessageSeconds(true);
	AyuSettings::save();
	auto saved = core.readPref<QByteArray>(kSettingsKey);
	const auto document = nlohmann::json::parse(saved.toStdString());
	Check(document.value("futureEnvelope", "") == "keep"
		&& document["settings"].value("future", "") == "preserve"
		&& document["settings"]["ghostModeSettings"]["0"]
			.value("futureGhost", "") == "keep",
		u"Unknown settings fields survive saves"_q);
	Check(Nagram::ForDevice().Get(Nagram::Messages::kSecondsInMessages),
		u"Advanced seconds toggle updates canonical setting"_q);
	Check(Nagram::ForDevice().Set(Nagram::Compose::kConfirmSticker, true),
		u"Canonical sticker confirmation written"_q);
	Check(AyuSettings::getInstance().stickerConfirmation(),
		u"Canonical setting updates advanced toggle"_q);
	saved = core.readPref<QByteArray>(kSettingsKey);
	const auto future = QByteArray(R"({"version":2,"settings":{"future":"data"}})");
	core.writePref<QByteArray>(kSettingsKey, future);
	AyuSettings::load();
	AyuSettings::save();
	Check(!AyuSettings::StorageAvailable()
		&& core.readPref<QByteArray>(kSettingsKey) == future,
		u"Future settings version is preserved"_q);
	core.writePref<QByteArray>(kSettingsKey, R"({
		"sendReadMessages":false,"sendReadStories":false,
		"sendOnlinePackets":false,"sendUploadProgress":false,
		"sendOfflinePacketAfterOnline":true,"markReadAfterAction":false
	})");
	AyuSettings::load();
	Check(AyuSettings::getInstance().useGlobalGhostMode()
		&& !AyuSettings::ghost(0).sendReadMessages()
		&& !AyuSettings::ghost(0).sendReadStories()
		&& !AyuSettings::ghost(0).sendOnlinePackets()
		&& !AyuSettings::ghost(0).markReadAfterAction(),
		u"Legacy ghost mode values migrate"_q);
	core.writePref<QByteArray>(kSettingsKey, saved);
	AyuSettings::load();
	AyuSettings::save();
	Core::App().saveSettings();
}

void WriteHistory() {
	Check(AyuDatabase::available(), u"Application history database opens"_q);
	if (!AyuDatabase::available()) {
		return;
	}
	AyuDatabase::removeDeletedMessage(kFirstUser, kDialog, 1);
	AyuDatabase::removeDeletedMessage(kSecondUser, kDialog, 2);
	auto message = DeletedMessage();
	message.userId = kFirstUser;
	message.dialogId = kDialog;
	message.messageId = 1;
	message.text = "fictional-regram-private-history";
	AyuDatabase::addDeletedMessage(message);
	message.userId = kSecondUser;
	message.messageId = 2;
	message.text = "fictional-second-account";
	AyuDatabase::addDeletedMessage(message);
	const auto rows = AyuDatabase::getDeletedMessages(kFirstUser, kDialog, 0, 0, 0, 10);
	Check(rows.size() == 1 && rows[0].text == "fictional-regram-private-history",
		u"History query uses account identity"_q);
	auto file = QFile(cWorkingDir() + u"tdata/regram-history.db"_q);
	Check(file.open(QIODevice::ReadOnly), u"History file can be inspected"_q);
	const auto bytes = file.readAll();
	Check(!bytes.startsWith("SQLite format 3")
		&& !bytes.contains("fictional-regram-private-history"),
		u"Application history file is encrypted"_q);
	Check(QFile::exists(cWorkingDir() + u"tdata/regram-history-key"_q)
		&& Core::App().settings().readPref<QByteArray>(
			"regram.archive.sqlcipher-key-v1").isEmpty(),
		u"History key is wrapped by local account encryption"_q);
	Core::App().saveSettings();
}

void ReadHistory() {
	Check(AyuDatabase::available(), u"History database reopens after restart"_q);
	if (!AyuDatabase::available()) {
		return;
	}
	const auto rows = AyuDatabase::getDeletedMessages(kFirstUser, kDialog, 0, 0, 0, 10);
	Check(rows.size() == 1 && rows[0].text == "fictional-regram-private-history",
		u"Application retained message survives restart"_q);
	AyuDatabase::removeDeletedMessage(kFirstUser, kDialog, 1);
	Check(!AyuDatabase::hasDeletedMessages(kFirstUser, kDialog, 0)
		&& AyuDatabase::hasDeletedMessages(kSecondUser, kDialog, 0),
		u"History deletion preserves another account"_q);
	AyuDatabase::removeDeletedMessage(kSecondUser, kDialog, 2);
	Check(AyuSettings::getInstance().showMessageSeconds()
		&& Nagram::ForDevice().Get(Nagram::Messages::kSecondsInMessages)
		&& AyuSettings::getInstance().stickerConfirmation(),
		u"Shared settings survive process restart"_q);
}

} // namespace

void VerifyRegramIntegration() {
	if (!DisposableProfile()) {
		Fail(u"Integration test requires an unauthenticated disposable profile"_q);
		return;
	}
	const auto stage = qEnvironmentVariable("REGRAM_M0_STAGE");
	if (stage == u"write"_q) {
		VerifySettings();
		WriteHistory();
	} else if (stage == u"read"_q) {
		ReadHistory();
	} else {
		Fail(u"Integration stage must be write or read"_q);
	}
}

void VerifyRegramBranding() {
	if (!Active() || !QFile::exists(cWorkingDir() + u"testing"_q)) {
		return;
	}
	const auto legacyText = [](const QString &text) {
		const auto lower = text.toLower();
		return lower.contains(u"ayugram"_q)
			|| lower.contains(u"nagram"_q)
			|| lower.contains(u"exteragram"_q);
	};
	auto pack = Lang::Instance();
	const auto key = QByteArray("ayu_AyuPreferences");
	const auto index = Lang::GetKeyIndex(QLatin1String(key));
	Check(index != Lang::kKeysCount, u"Branding key is available"_q);
	if (index == Lang::kKeysCount) {
		return;
	}
	for (const auto &value : { "AyuGram preferences", "NAGRAM preferences", "exteraGram preferences" }) {
		pack.applyValue(key, value);
		Check(pack.getNonDefaultValue(key).isEmpty()
			&& !legacyText(pack.getValue(index)),
			u"Legacy translation falls back to Regram text"_q);
	}
	pack.applyValue(key, "Translated Regram preferences");
	Check(pack.getValue(index) == u"Translated Regram preferences"_q,
		u"Translations without legacy branding remain available"_q);
	auto cached = QByteArray();
	{
		auto stream = QDataStream(&cached, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << u"#new"_q << qint32(1)
			<< u"en"_q << u"en"_q << QString() << QString()
			<< qint32(0) << QString() << QString() << QByteArray()
			<< qint32(1) << key << QByteArray("AyuGram preferences")
			<< QByteArray();
	}
	pack.fillFromSerialized(cached, AppVersion);
	Check(pack.getNonDefaultValue(key).isEmpty()
		&& !legacyText(pack.getValue(index)),
		u"Cached language pack cannot restore legacy branding"_q);
	for (auto i = 0; i != Lang::kKeysCount; ++i) {
		if (legacyText(pack.getValue(ushort(i)))) {
			Fail(u"Compiled language contains legacy branding"_q,
				u"key=%1"_q.arg(i));
			return;
		}
	}
	Pass(u"All compiled display strings have no legacy branding"_q);
	auto &live = Lang::GetInstance();
	const auto saved = live.serialize();
	const auto restore = gsl::finally([&] {
		live.fillFromSerialized(saved, AppVersion);
	});
	for (const auto &id : { u"en"_q, u"zh-hans"_q, u"zh-hant"_q }) {
		live.switchToId({ .id = id });
		Check(!legacyText(tr::lng_nagram_about(tr::now))
			&& tr::lng_nagram_settings(tr::now).contains(u"Regram"_q),
			u"Bundled language keeps Regram identity"_q, id);
	}
	live.applyValue("lng_quit_from_tray", "Quit Telegram");
	Check(tr::lng_quit_from_tray(tr::now) == u"Quit Regram"_q,
		u"Cloud translations retain Regram application names"_q);
	Check(Nagram::Sync::FileName() == u"regram-sync-v1.json"_q
		&& Nagram::Sync::Caption() == u"#regram_sync"_q,
		u"New synchronized backups use Regram names"_q);
	const auto envelope = Nagram::Sync::EncodeEnvelope(
		QByteArray(R"({"version":1,"options":{}})"), 1, u"Regram"_q);
	Check(Nagram::Sync::DecodeEnvelope(envelope).error
		== Nagram::Sync::EnvelopeError::None,
		u"Backup format remains compatible after the display rename"_q);
	const auto source = Nagram::Interface::AppIconPreview(QString());
	Check(!source.isNull(), u"Regram icon source is available"_q);
	for (const auto &name : { AyuAssets::ALT_ICON, AyuAssets::DISCORD_ICON,
		AyuAssets::SPOTIFY_ICON, AyuAssets::EXTERA_ICON, AyuAssets::NOTHING_ICON,
		AyuAssets::BARD_ICON, AyuAssets::YAPLUS_ICON, AyuAssets::WIN95_ICON,
		AyuAssets::CHIBI_ICON, AyuAssets::CHIBI2_ICON, AyuAssets::EXTERA2_ICON }) {
		const auto theme = Nagram::Interface::AppIconPreview(name);
		auto sameGlyph = !theme.isNull() && theme.size() == source.size();
		for (auto y = source.height() / 4; sameGlyph && y != 3 * source.height() / 4; ++y) {
			for (auto x = source.width() / 4; sameGlyph && x != 3 * source.width() / 4; ++x) {
				const auto pixel = source.pixelColor(x, y);
				if (pixel.alpha() == 255 && pixel.red() == 255
					&& pixel.green() == 255 && pixel.blue() == 255) {
					sameGlyph = theme.pixelColor(x, y) == pixel;
				}
			}
		}
		Check(sameGlyph && !AyuAssets::loadPreview(name).isNull(),
			u"Alternative icon retains the Regram artwork"_q, name);
	}
	const auto ico = AyuAssets::EncodeIco(source);
	auto reader = QDataStream(ico);
	reader.setByteOrder(QDataStream::LittleEndian);
	auto reserved = quint16(0);
	auto type = quint16(0);
	auto count = quint16(0);
	auto width = quint8(1);
	auto height = quint8(1);
	auto colors = quint8(1);
	auto entryReserved = quint8(1);
	auto planes = quint16(0);
	auto depth = quint16(0);
	auto size = quint32(0);
	auto offset = quint32(0);
	reader >> reserved >> type >> count >> width >> height >> colors
		>> entryReserved >> planes >> depth >> size >> offset;
	const auto embedded = QImage::fromData(ico.mid(offset), "PNG");
	Check(reader.status() == QDataStream::Ok
		&& !reserved && type == 1 && count == 1
		&& !width && !height && !colors && !entryReserved
		&& planes == 1 && depth == 32 && offset == 22
		&& size == ico.size() - offset
		&& embedded.size() == QSize(256, 256),
		u"Regram artwork supports Windows icon export"_q);

}

} // namespace Test

#endif // _DEBUG
