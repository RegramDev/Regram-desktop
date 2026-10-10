// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/data/ayu_database.h"

#include "regram/ayu/data/entities.h"
#include "regram/archive/snapshot.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "storage/storage_account.h"
#include "regram/ayu/libs/sqlite/sqlite_orm.h"
#include "base/unixtime.h"
#include "base/random.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "storage/details/storage_file_utilities.h"

#include <atomic>
#include <stdexcept>

using namespace sqlite_orm;

namespace {

std::atomic<bool> DatabaseReady = false;

QByteArray RetentionKey() {
	constexpr auto kLegacyKey = "regram.archive.sqlcipher-key-v1";
	const auto localKey = Core::App().domain().active().local().peekLegacyLocalKey();
	if (!localKey) {
		throw std::runtime_error("Local account encryption is unavailable");
	}
	const auto keyFile = Regram::Archive::Snapshot(
		cWorkingDir() + u"tdata/regram-history-key"_q, 0x52474B31, localKey);
	const auto previous = keyFile.read();
	using Status = Regram::Archive::Status;
	if (previous.status == Status::Success) {
		if (previous.payload.size() != 32) {
			throw std::runtime_error("Invalid retention encryption key");
		}
		auto &settings = Core::App().settings();
		if (settings.readPref<QByteArray>(kLegacyKey) == previous.payload) {
			settings.clearPref(kLegacyKey);
			Core::App().saveSettings();
			Storage::details::Sync();
		}
		return previous.payload;
	} else if (previous.status != Status::Missing) {
		throw std::runtime_error("Retention encryption key file is unavailable");
	}
	auto &settings = Core::App().settings();
	const auto legacy = settings.readPref<QByteArray>(kLegacyKey);
	if ((!legacy.isEmpty() && legacy.size() != 32)
		|| (legacy.isEmpty() && QFile::exists(u"./tdata/regram-history.db"_q))) {
		throw std::runtime_error("Retention encryption key is unavailable");
	}
	auto key = legacy;
	if (key.isEmpty()) {
		key = QByteArray(32, Qt::Uninitialized);
		base::RandomFill(key.data(), key.size());
	}
	Storage::details::Sync();
	if (keyFile.write(key) != Status::Success) {
		throw std::runtime_error("Retention encryption key could not be saved");
	}
	const auto saved = keyFile.read();
	if (saved.status != Status::Success || saved.payload != key) {
		throw std::runtime_error("Retention encryption key could not be verified");
	}
	if (!legacy.isEmpty()) {
		settings.clearPref(kLegacyKey);
		Core::App().saveSettings();
		Storage::details::Sync();
	}
	return key;
}

} // namespace
auto MakeStorage() {
	return make_storage(
	"./tdata/regram-history.db",
	make_table<SchemaVersion>(
		"SchemaVersion",
		make_column("id", &SchemaVersion::id, primary_key()),
		make_column("version", &SchemaVersion::version)
	),
	make_index("idx_deleted_message_userId_dialogId_topicId_messageId",
			   column<DeletedMessage>(&DeletedMessage::userId),
			   column<DeletedMessage>(&DeletedMessage::dialogId),
			   column<DeletedMessage>(&DeletedMessage::topicId),
			   column<DeletedMessage>(&DeletedMessage::messageId)),
	make_index("idx_edited_message_userId_dialogId_messageId",
			   column<EditedMessage>(&EditedMessage::userId),
			   column<EditedMessage>(&EditedMessage::dialogId),
			   column<EditedMessage>(&EditedMessage::messageId)),
	make_table<DeletedMessage>(
		"DeletedMessage",
		make_column("fakeId", &DeletedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedMessage::userId),
		make_column("dialogId", &DeletedMessage::dialogId),
		make_column("groupedId", &DeletedMessage::groupedId),
		make_column("peerId", &DeletedMessage::peerId),
		make_column("fromId", &DeletedMessage::fromId),
		make_column("topicId", &DeletedMessage::topicId),
		make_column("messageId", &DeletedMessage::messageId),
		make_column("date", &DeletedMessage::date),
		make_column("flags", &DeletedMessage::flags),
		make_column("editDate", &DeletedMessage::editDate),
		make_column("views", &DeletedMessage::views),
		make_column("fwdFlags", &DeletedMessage::fwdFlags),
		make_column("fwdFromId", &DeletedMessage::fwdFromId),
		make_column("fwdName", &DeletedMessage::fwdName),
		make_column("fwdDate", &DeletedMessage::fwdDate),
		make_column("fwdPostAuthor", &DeletedMessage::fwdPostAuthor),
		make_column("replyFlags", &DeletedMessage::replyFlags),
		make_column("replyMessageId", &DeletedMessage::replyMessageId),
		make_column("replyPeerId", &DeletedMessage::replyPeerId),
		make_column("replyTopId", &DeletedMessage::replyTopId),
		make_column("replyForumTopic", &DeletedMessage::replyForumTopic),
		make_column("replySerialized", &DeletedMessage::replySerialized),
		make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
		make_column("text", &DeletedMessage::text),
		make_column("textEntities", &DeletedMessage::textEntities),
		make_column("mediaPath", &DeletedMessage::mediaPath),
		make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
		make_column("documentType", &DeletedMessage::documentType),
		make_column("documentSerialized", &DeletedMessage::documentSerialized),
		make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &DeletedMessage::documentAttributesSerialized),
		make_column("mimeType", &DeletedMessage::mimeType)
	),
	make_table<EditedMessage>(
		"EditedMessage",
		make_column("fakeId", &EditedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &EditedMessage::userId),
		make_column("dialogId", &EditedMessage::dialogId),
		make_column("groupedId", &EditedMessage::groupedId),
		make_column("peerId", &EditedMessage::peerId),
		make_column("fromId", &EditedMessage::fromId),
		make_column("topicId", &EditedMessage::topicId),
		make_column("messageId", &EditedMessage::messageId),
		make_column("date", &EditedMessage::date),
		make_column("flags", &EditedMessage::flags),
		make_column("editDate", &EditedMessage::editDate),
		make_column("views", &EditedMessage::views),
		make_column("fwdFlags", &EditedMessage::fwdFlags),
		make_column("fwdFromId", &EditedMessage::fwdFromId),
		make_column("fwdName", &EditedMessage::fwdName),
		make_column("fwdDate", &EditedMessage::fwdDate),
		make_column("fwdPostAuthor", &EditedMessage::fwdPostAuthor),
		make_column("replyFlags", &EditedMessage::replyFlags),
		make_column("replyMessageId", &EditedMessage::replyMessageId),
		make_column("replyPeerId", &EditedMessage::replyPeerId),
		make_column("replyTopId", &EditedMessage::replyTopId),
		make_column("replyForumTopic", &EditedMessage::replyForumTopic),
		make_column("replySerialized", &EditedMessage::replySerialized),
		make_column("entityCreateDate", &EditedMessage::entityCreateDate),
		make_column("text", &EditedMessage::text),
		make_column("textEntities", &EditedMessage::textEntities),
		make_column("mediaPath", &EditedMessage::mediaPath),
		make_column("hqThumbPath", &EditedMessage::hqThumbPath),
		make_column("documentType", &EditedMessage::documentType),
		make_column("documentSerialized", &EditedMessage::documentSerialized),
		make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &EditedMessage::documentAttributesSerialized),
		make_column("mimeType", &EditedMessage::mimeType)
	),
	make_table<DeletedDialog>(
		"DeletedDialog",
		make_column("fakeId", &DeletedDialog::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedDialog::userId),
		make_column("dialogId", &DeletedDialog::dialogId),
		make_column("peerId", &DeletedDialog::peerId),
		make_column("folderId", &DeletedDialog::folderId),
		make_column("topMessage", &DeletedDialog::topMessage),
		make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
		make_column("flags", &DeletedDialog::flags),
		make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
	),
	make_table<RegexFilter>(
		"RegexFilter",
		make_column("id", &RegexFilter::id, primary_key()),
		make_column("text", &RegexFilter::text),
		make_column("enabled", &RegexFilter::enabled),
		make_column("reversed", &RegexFilter::reversed),
		make_column("caseInsensitive", &RegexFilter::caseInsensitive),
		make_column("dialogId", &RegexFilter::dialogId)
	),
	make_table<RegexFilterGlobalExclusion>(
		"RegexFilterGlobalExclusion",
		make_column("fakeId", &RegexFilterGlobalExclusion::fakeId, primary_key().autoincrement()),
		make_column("dialogId", &RegexFilterGlobalExclusion::dialogId),
		make_column("filterId", &RegexFilterGlobalExclusion::filterId)
	),
	make_table<SpyMessageRead>(
		"SpyMessageRead",
		make_column("fakeId", &SpyMessageRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageRead::userId),
		make_column("dialogId", &SpyMessageRead::dialogId),
		make_column("messageId", &SpyMessageRead::messageId),
		make_column("entityCreateDate", &SpyMessageRead::entityCreateDate)
	),
	make_table<SpyMessageContentsRead>(
		"SpyMessageContentsRead",
		make_column("fakeId", &SpyMessageContentsRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageContentsRead::userId),
		make_column("dialogId", &SpyMessageContentsRead::dialogId),
		make_column("messageId", &SpyMessageContentsRead::messageId),
		make_column("entityCreateDate", &SpyMessageContentsRead::entityCreateDate)
	)
	);
}

namespace {

using StorageType = decltype(MakeStorage());
std::unique_ptr<StorageType> DatabaseStorage;

StorageType &Database() {
	if (!DatabaseStorage) {
		DatabaseStorage = std::make_unique<StorageType>(MakeStorage());
	}
	return *DatabaseStorage;
}

} // namespace

namespace AyuMigrations {

void migrateToV1(StorageType &storage) {
    storage.sync_schema(true);
}

}

void runMigrations(StorageType &storage) {
	constexpr auto kLatestVersion = 1;
	const auto version = storage.get_pointer<SchemaVersion>(1);
	const auto current = version ? version->version : 0;
	if (current < 0 || current > kLatestVersion) {
		throw std::runtime_error("Unsupported retention database version");
	}
	if (!version) {
		storage.insert(SchemaVersion{1, 0});
	}
	for (auto next = current + 1; next <= kLatestVersion; ++next) {
		if (!storage.transaction([&] {
			AyuMigrations::migrateToV1(storage);
			storage.update_all(set(c(&SchemaVersion::version) = next),
				where(c(&SchemaVersion::id) == 1));
			return true;
		})) {
			throw std::runtime_error("Retention database migration failed");
		}
	}
}

namespace AyuDatabase {

void shutdown() {
	DatabaseReady = false;
	DatabaseStorage.reset();
}

bool available() {
	return DatabaseReady;
}

void moveCurrentDatabase() {
	DatabaseReady = false;
	LOG(("Regram retention database preserved for recovery."));
}

void initialize() {
	DatabaseReady = false;
	try {
		const auto key = RetentionKey();
		Database().on_open = [key](sqlite3 *database) {
			const auto status = sqlite3_key(database, key.constData(), key.size());
			if (status != SQLITE_OK) {
				throw std::runtime_error("Retention database could not be keyed");
			}
		};
		Database().open_forever();
		if (Database().table_exists("SchemaVersion")) {
			if (const auto version = Database().get_pointer<SchemaVersion>(1)) {
				if (version->version < 0 || version->version > 1) {
					throw std::runtime_error("Unsupported retention database version");
				}
			}
		}
		Database().sync_schema(true);
		runMigrations(Database());
		DatabaseReady = true;
	} catch (const std::exception &error) {
		LOG(("Regram retention database unavailable: %1").arg(error.what()));
	}
}

void addEditedMessage(const EditedMessage &message) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().begin_transaction();
		Database().insert(message);
		Database().commit();
	} catch (std::exception &ex) {
		try {
			Database().rollback();
		} catch (...) {
		}
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit) {
	if (!DatabaseReady) {
		return {};
	}

	return Database().get_all<EditedMessage>(
		where(
			column<EditedMessage>(&EditedMessage::userId) == userId and
			column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
			column<EditedMessage>(&EditedMessage::messageId) == messageId and
			(column<EditedMessage>(&EditedMessage::fakeId) > minId or minId == 0) and
			(column<EditedMessage>(&EditedMessage::fakeId) < maxId or maxId == 0)
		),
		order_by(column<EditedMessage>(&EditedMessage::fakeId)).desc(),
		limit(totalLimit)
	);
}

bool hasRevisions(ID userId, ID dialogId, ID messageId) {
	if (!DatabaseReady) {
		return false;
	}

	try {
		return !Database().select(
			columns(column<EditedMessage>(&EditedMessage::messageId)),
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if message has revisions: %1").arg(ex.what()));
		return false;
	}
}

void addDeletedMessage(const DeletedMessage &message) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().begin_transaction();
		Database().insert(message);
		Database().commit();
	} catch (std::exception &ex) {
		try {
			Database().rollback();
		} catch (...) {
		}
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit, const std::string &searchQuery) {
	if (!DatabaseReady) {
		return {};
	}

	if (searchQuery.empty()) {
		return Database().get_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0)
			),
			order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
			limit(totalLimit)
		);
	}

	std::string escaped;
	escaped.reserve(searchQuery.size());
	for (const auto c : searchQuery) {
		if (c == '%' || c == '_' || c == '\\') {
			escaped += '\\';
		}
		escaped += c;
	}
	const auto pattern = "%" + escaped + "%";
	return Database().get_all<DeletedMessage>(
		where(
			column<DeletedMessage>(&DeletedMessage::userId) == userId and
			column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
			(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0) and
			like(column<DeletedMessage>(&DeletedMessage::text), pattern, "\\")
		),
		order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
		limit(totalLimit)
	);
}

bool hasDeletedMessages(ID userId, ID dialogId, ID topicId) {
	if (!DatabaseReady) {
		return false;
	}

	try {
		return !Database().select(
			columns(column<DeletedMessage>(&DeletedMessage::dialogId)),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if dialog has deleted message: %1").arg(ex.what()));
		return false;
	}
}

void removeDeletedMessage(ID userId, ID dialogId, ID messageId) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				column<DeletedMessage>(&DeletedMessage::messageId) == messageId
			)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to remove deleted message: %1").arg(ex.what()));
	}
}

void clearDeletedMessages(ID userId, ID dialogId, ID topicId) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			)
		);
	} catch (std::exception &) {
	}
}

template<typename T>
std::vector<T> getAllT() {
	if (!DatabaseReady) {
		return {};
	}

	try {
		return Database().get_all<T>();
	} catch (std::exception &ex) {
		LOG(("Failed to get all: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getAllRegexFilters() {
	if (!DatabaseReady) {
		return {};
	}

	return getAllT<RegexFilter>();
}

std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions() {
	if (!DatabaseReady) {
		return {};
	}

	return getAllT<RegexFilterGlobalExclusion>();
}

std::vector<RegexFilter> getExcludedByDialogId(ID dialogId) {
	if (!DatabaseReady) {
		return {};
	}

	try {
		return Database().get_all<RegexFilter>(
			where(in(&RegexFilter::id,
					 Database().select(columns(&RegexFilterGlobalExclusion::filterId),
									where(is_equal(&RegexFilterGlobalExclusion::dialogId, dialogId))
					 )
			))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get excluded by dialog id: %1").arg(ex.what()));
		return {};
	}
}

int getCount() {
	if (!DatabaseReady) {
		return 0;
	}

	try {
		return Database().count<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to get count: %1").arg(ex.what()));
		return 0;
	}
}

RegexFilter getById(std::vector<char> id) {
	if (!DatabaseReady) {
		return {};
	}

	try {
		return Database().get<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == std::move(id))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by id: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getShared() {
	if (!DatabaseReady) {
		return {};
	}

	try {
		return Database().get_all<RegexFilter>(
			where(is_null(column<RegexFilter>(&RegexFilter::dialogId)))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get shared filters: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getByDialogId(ID dialogId) {
	if (!DatabaseReady) {
		return {};
	}

	try {
		return Database().get_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::dialogId) == dialogId)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by dialog id: %1").arg(ex.what()));
		return {};
	}
}

void addRegexFilter(const RegexFilter &filter) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().begin_transaction();
		Database().replace(filter); // we're using replace as we set std::vector<char> as primary key
		Database().commit();
	} catch (std::exception &ex) {
		try {
			Database().rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter for some reason: %1").arg(ex.what()));
	}
}

void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().begin_transaction();
		Database().insert(exclusion);
		Database().commit();
	} catch (std::exception &ex) {
		try {
			Database().rollback();
		} catch (...) {
		}
		LOG(("Failed to save regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void updateRegexFilter(const RegexFilter &filter) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().update_all(
			set(
				c(&RegexFilter::text) = filter.text,
				c(&RegexFilter::enabled) = filter.enabled,
				c(&RegexFilter::reversed) = filter.reversed,
				c(&RegexFilter::caseInsensitive) = filter.caseInsensitive,
				c(&RegexFilter::dialogId) = filter.dialogId
			),
			where(c(&RegexFilter::id) == filter.id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to update regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteFilter(const std::vector<char> &id) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusionsByFilterId(const std::vector<char> &id) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == id)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion by filter id for some reason: %1").arg(ex.what()));
	}
}

void deleteExclusion(ID dialogId, std::vector<char> filterId) {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<RegexFilterGlobalExclusion>(
			where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == filterId and
				column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::dialogId) == dialogId
			)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to delete regex filter exclusion for some reason: %1").arg(ex.what()));
	}
}

void deleteAllFilters() {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter for some reason: %1").arg(ex.what()));
	}
}

void deleteAllExclusions() {
	if (!DatabaseReady) {
		return;
	}

	try {
		Database().remove_all<RegexFilterGlobalExclusion>();
	} catch (std::exception &ex) {
		LOG(("Failed to delete all regex filter exclusions for some reason: %1").arg(ex.what()));
	}
}

bool hasFilters() {
	if (!DatabaseReady) {
		return false;
	}

	try {
		return !Database().select(
			columns(column<RegexFilter>(&RegexFilter::id)),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

bool hasPerDialogFilters() {
	if (!DatabaseReady) {
		return false;
	}

	try {
		return
			!Database().select(
				columns(column<RegexFilter>(&RegexFilter::id)),
				where(is_not_null(column<RegexFilter>(&RegexFilter::dialogId))),
				limit(1)
			).empty() ||
			!Database().select(
				columns(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::fakeId)),
				limit(1)
			).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

}
