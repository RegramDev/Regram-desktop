#include "regram/nagram/sync/backend.h"

#include "regram/nagram/sync/model.h"

#include "apiwrap.h"
#include "base/random.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_histories.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "storage/file_upload.h"
#include "storage/localimageloader.h"

#include <QtCore/QFile>

namespace Nagram::Sync {
namespace {

constexpr auto kSearchLimit = 100;

class SavedMessages final : public Backend {
public:
	explicit SavedMessages(not_null<Main::Session*> session);
	~SavedMessages();

	void fetch(quint64 knownId, Fn<void(Fetched)> done) override;
	void upload(const QByteArray &data, Fn<void(Uploaded)> done) override;
	void remove(const std::vector<quint64> &ids) override;
	void cancel() override;

private:
	[[nodiscard]] bool isBackup(not_null<HistoryItem*> item) const;
	[[nodiscard]] std::vector<quint64> collect(
		const MTPmessages_Messages &result) const;
	void search(const QString &caption, std::vector<quint64> ids);
	void found(std::vector<quint64> ids);
	void download(std::vector<quint64> ids);
	void finishDownload(
		std::vector<quint64> ids,
		not_null<DocumentData*> document);
	void send(const MTPInputFile &file);
	void fetched(Fetched result);
	void uploaded(Uploaded result);

	const not_null<Main::Session*> _session;
	MTP::Sender _api;
	mtpRequestId _requestId = 0;
	FullMsgId _uploadId;
	quint64 _knownId = 0;
	std::shared_ptr<Data::DocumentMedia> _media;
	Fn<void(Fetched)> _fetched;
	Fn<void(Uploaded)> _uploaded;
	rpl::lifetime _lifetime;

};

SavedMessages::SavedMessages(not_null<Main::Session*> session)
: _session(session)
, _api(&session->mtp()) {
}

SavedMessages::~SavedMessages() {
	cancel();
}

void SavedMessages::cancel() {
	_lifetime.destroy();
	_media = nullptr;
	_fetched = nullptr;
	_uploaded = nullptr;
	_api.request(base::take(_requestId)).cancel();
	if (const auto id = base::take(_uploadId)) {
		_session->uploader().cancel(id);
	}
}

void SavedMessages::fetched(Fetched result) {
	const auto done = base::take(_fetched);
	cancel();
	if (done) {
		done(std::move(result));
	}
}

void SavedMessages::uploaded(Uploaded result) {
	const auto done = base::take(_uploaded);
	cancel();
	if (done) {
		done(result);
	}
}

bool SavedMessages::isBackup(not_null<HistoryItem*> item) const {
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	return document
		&& item->isRegular()
		&& item->history()->peer->isSelf()
		&& !item->Has<HistoryMessageForwarded>()
		&& ((document->filename() == FileName()
			&& item->originalText().text == Caption())
			|| (document->filename() == u"nagram-sync-v1.json"_q
				&& item->originalText().text == u"#nagram_sync"_q));
}

std::vector<quint64> SavedMessages::collect(
		const MTPmessages_Messages &result) const {
	_session->data().processExistingMessages(nullptr, result);
	const auto list = result.match([](
			const MTPDmessages_messagesNotModified &) {
		return QVector<MTPMessage>();
	}, [](const auto &data) {
		return data.vmessages().v;
	});
	auto ids = std::vector<quint64>();
	for (const auto &message : list) {
		const auto item = _session->data().message(
			PeerFromMessage(message),
			IdFromMessage(message));
		if (item && isBackup(item)) {
			ids.push_back(quint64(item->id.bare));
		}
	}
	return ids;
}

void SavedMessages::fetch(quint64 knownId, Fn<void(Fetched)> done) {
	cancel();
	_fetched = std::move(done);
	_knownId = knownId;
	search(Caption(), {});
}

void SavedMessages::search(
		const QString &caption,
		std::vector<quint64> ids) {
	_requestId = _api.request(MTPmessages_Search(
		MTP_flags(0),
		_session->user()->input(),
		MTP_string(caption),
		MTP_inputPeerEmpty(),
		MTP_inputPeerEmpty(),
		MTPVector<MTPReaction>(),
		MTP_int(0), // top_msg_id
		MTP_inputMessagesFilterDocument(),
		MTP_int(0), // min_date
		MTP_int(0), // max_date
		MTP_int(0), // offset_id
		MTP_int(0), // add_offset
		MTP_int(kSearchLimit),
		MTP_int(0), // max_id
		MTP_int(0), // min_id
		MTP_long(0) // hash
	)).done([=](const MTPmessages_Messages &result) {
		_requestId = 0;
		auto all = ids;
		const auto collected = collect(result);
		all.insert(all.end(), collected.begin(), collected.end());
		if (caption == Caption()) {
			search(u"#nagram_sync"_q, std::move(all));
		} else {
			found(std::move(all));
		}
	}).fail([=] {
		_requestId = 0;
		fetched({ .error = BackendError::Search });
	}).send();
}

void SavedMessages::found(std::vector<quint64> ids) {
	if (!_knownId || ranges::contains(ids, _knownId)) {
		download(std::move(ids));
		return;
	}
	_requestId = _api.request(MTPmessages_GetMessages(
		MTP_vector<MTPInputMessage>(
			1,
			MTP_inputMessageID(MTP_int(int(_knownId))))
	)).done([=](const MTPmessages_Messages &result) {
		_requestId = 0;
		auto all = ids;
		for (const auto id : collect(result)) {
			all.push_back(id);
		}
		download(std::move(all));
	}).fail([=] {
		_requestId = 0;
		fetched({ .error = BackendError::Search });
	}).send();
}

void SavedMessages::download(std::vector<quint64> ids) {
	if (ids.empty()) {
		fetched({});
		return;
	}
	const auto latest = *ranges::max_element(ids);
	const auto item = _session->data().message(
		_session->userPeerId(),
		MsgId(int64(latest)));
	const auto document = (item && item->media())
		? item->media()->document()
		: nullptr;
	if (!document) {
		fetched({ .error = BackendError::Download });
		return;
	} else if (document->size > kMaxBackupBytes) {
		fetched({ .error = BackendError::TooLarge, .all = std::move(ids) });
		return;
	}
	_media = document->createMediaView();
	document->save(item->fullId(), QString());
	if (_media->loaded(true)) {
		finishDownload(std::move(ids), document);
		return;
	}
	_session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		if (_media->loaded(true)) {
			finishDownload(ids, document);
		} else if (!document->loading()) {
			fetched({ .error = BackendError::Download, .all = ids });
		}
	}, _lifetime);
}

void SavedMessages::finishDownload(
		std::vector<quint64> ids,
		not_null<DocumentData*> document) {
	auto bytes = _media->bytes();
	if (bytes.isEmpty()) {
		auto file = QFile(document->filepath(true));
		if (file.open(QIODevice::ReadOnly)) {
			bytes = file.read(kMaxBackupBytes + 1);
		}
	}
	if (bytes.isEmpty()) {
		fetched({ .error = BackendError::Download, .all = std::move(ids) });
		return;
	}
	const auto latest = *ranges::max_element(ids);
	fetched({
		.found = true,
		.id = latest,
		.data = std::move(bytes),
		.all = std::move(ids),
	});
}

void SavedMessages::upload(const QByteArray &data, Fn<void(Uploaded)> done) {
	cancel();
	_uploaded = std::move(done);
	if (data.isEmpty() || data.size() > kMaxBackupBytes) {
		uploaded({ .error = BackendError::TooLarge });
		return;
	}
	auto file = MakePreparedFile({
		.id = base::RandomValue<uint64>(),
		.type = SendMediaType::SecondaryFile,
	});
	file->filename = FileName();
	file->filemime = u"application/json"_q;
	file->filesize = int64(data.size());
	file->setFileData(data);
	_uploadId = FullMsgId(
		_session->userPeerId(),
		_session->data().nextLocalMessageId());
	const auto id = _uploadId;
	auto &uploader = _session->uploader();
	uploader.secondaryFileReady(
	) | rpl::filter([=](const Storage::UploadedMedia &media) {
		return (media.fullId == id);
	}) | rpl::on_next([=](const Storage::UploadedMedia &media) {
		const auto file = media.info.file;
		_uploadId = FullMsgId();
		_lifetime.destroy();
		send(file);
	}, _lifetime);
	uploader.secondaryFileFailed(
	) | rpl::filter([=](FullMsgId failed) {
		return (failed == id);
	}) | rpl::on_next([=] {
		_uploadId = FullMsgId();
		uploaded({ .error = BackendError::Upload });
	}, _lifetime);
	uploader.upload(id, file);
}

void SavedMessages::send(const MTPInputFile &file) {
	using Flag = MTPmessages_SendMedia::Flag;
	const auto randomId = base::RandomValue<uint64>();
	const auto caption = Caption();
	_requestId = _api.request(MTPmessages_SendMedia(
		MTP_flags(Flag::f_silent | Flag::f_entities),
		_session->user()->input(),
		MTPInputReplyTo(),
		MTP_inputMediaUploadedDocument(
			MTP_flags(MTPDinputMediaUploadedDocument::Flag::f_force_file),
			file,
			MTPInputFile(), // thumb
			MTP_string(u"application/json"_q),
			MTP_vector<MTPDocumentAttribute>(
				1,
				MTP_documentAttributeFilename(MTP_string(FileName()))),
			MTPVector<MTPInputDocument>(),
			MTPInputPhoto(), // video_cover
			MTPint(), // video_timestamp
			MTPint()), // ttl_seconds
		MTP_string(caption),
		MTP_long(randomId),
		MTPReplyMarkup(),
		MTP_vector<MTPMessageEntity>(
			1,
			MTP_messageEntityHashtag(
				MTP_int(0),
				MTP_int(caption.size()))),
		MTPint(), // schedule_date
		MTPint(), // schedule_repeat_period
		MTPInputPeer(), // send_as
		MTPInputQuickReplyShortcut(),
		MTPlong(), // effect
		MTPlong(), // allow_paid_stars
		MTPSuggestedPost()
	)).done([=](const MTPUpdates &result) {
		_requestId = 0;
		auto id = quint64(0);
		const auto check = [&](const MTPUpdate &update) {
			update.match([&](const MTPDupdateMessageID &data) {
				if (uint64(data.vrandom_id().v) == randomId) {
					id = quint64(data.vid().v);
				}
			}, [](const auto &) {
			});
		};
		result.match([&](const MTPDupdates &data) {
			ranges::for_each(data.vupdates().v, check);
		}, [&](const MTPDupdatesCombined &data) {
			ranges::for_each(data.vupdates().v, check);
		}, [&](const MTPDupdateShort &data) {
			check(data.vupdate());
		}, [&](const MTPDupdateShortSentMessage &data) {
			id = quint64(data.vid().v);
		}, [](const auto &) {
		});
		_session->api().applyUpdates(result);
		uploaded({ .id = id });
	}).fail([=] {
		_requestId = 0;
		uploaded({ .error = BackendError::Send });
	}).send();
}

void SavedMessages::remove(const std::vector<quint64> &ids) {
	auto list = MessageIdsList();
	for (const auto id : ids) {
		const auto item = _session->data().message(
			_session->userPeerId(),
			MsgId(int64(id)));
		if (item && isBackup(item)) {
			list.push_back(item->fullId());
		}
	}
	if (!list.empty()) {
		_session->data().histories().deleteMessages(list, true);
	}
}

} // namespace

std::unique_ptr<Backend> MakeSavedMessagesBackend(
		not_null<Main::Session*> session) {
	return std::make_unique<SavedMessages>(session);
}

} // namespace Nagram::Sync
