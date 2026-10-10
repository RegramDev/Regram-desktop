#include "regram/nagram/services/transcription.h"

#include "api/api_transcribes.h"
#include "apiwrap.h"
#include "base/weak_ptr.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "regram/nagram/core/options.h"
#include "regram/nagram/display/view_refresher.h"
#include "regram/nagram/menu/actions.h"
#include "regram/nagram/privacy/protection.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/services/request.h"
#include "regram/nagram/services/transcription_queue.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <map>
#include <memory>

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Nagram {
namespace {

constexpr auto kMaximumText = 16384;
constexpr auto kMaximumEntries = 128;
constexpr auto kDownloadTimeout = std::chrono::seconds(120);

[[nodiscard]] bool TranscriptionServiceSelected() {
	const auto config = Services();
	if (!config) {
		return false;
	}
	const auto id = config->value(u"transcription"_q).toString();
	return !id.isEmpty() && id != u"telegram"_q;
}

class ExternalTranscriptions final {
public:
	explicit ExternalTranscriptions(not_null<Main::Session*> session);

	[[nodiscard]] static ExternalTranscriptions &For(
		not_null<Main::Session*> session);

	[[nodiscard]] bool selected() const {
		return _selected;
	}
	[[nodiscard]] const Api::Transcribes::Entry *find(
		not_null<HistoryItem*> item) const;
	[[nodiscard]] bool toggle(not_null<HistoryItem*> item);
	[[nodiscard]] bool set(
		not_null<HistoryItem*> item,
		DocumentId documentId,
		const QByteArray &serviceConfig,
		uint64 generation,
		QString result);
	[[nodiscard]] uint64 generation(bool round) const {
		return _generation[round ? 1 : 0];
	}

private:
	struct Cached {
		DocumentId documentId = 0;
		Api::Transcribes::Entry value;
		uint64 accessed = 0;
	};

	void refresh(FullMsgId id);
	void clear();

	const not_null<Main::Session*> _session;
	base::flat_map<FullMsgId, Cached> _entries;
	QByteArray _config;
	uint64 _accessed = 0;
	std::array<uint64, 2> _generation = {};
	bool _selected = false;
	rpl::lifetime _lifetime;
};

class AudioDownload final {
public:
	AudioDownload();
	~AudioDownload();
	void start(
		not_null<Main::Session*> session,
		FullMsgId id,
		DocumentId documentId,
		std::shared_ptr<Data::DocumentMedia> media,
		Fn<void(QByteArray)> done,
		Fn<void(QString)> failed);
	void cancel();

private:
	void check();
	void fail(QString error);

	base::weak_ptr<Main::Session> _session;
	FullMsgId _id;
	DocumentId _documentId = 0;
	std::shared_ptr<Data::DocumentMedia> _media;
	Fn<void(QByteArray)> _done;
	Fn<void(QString)> _failed;
	QTimer _deadline;
	bool _ownsDownload = false;
	rpl::lifetime _lifetime;

};

ExternalTranscriptions::ExternalTranscriptions(
	not_null<Main::Session*> session)
: _session(session)
, _config(ForDevice().Get(kServicesConfig))
, _selected(TranscriptionServiceSelected()) {
	ForDevice().Value(kServicesConfig) | rpl::on_next([=](
			const QByteArray &config) {
		if (_config == config) {
			return;
		}
		_config = config;
		_selected = TranscriptionServiceSelected();
		clear();
		ViewRefresher::Refresh(_session->data());
	}, _lifetime);
}

ExternalTranscriptions &ExternalTranscriptions::For(
		not_null<Main::Session*> session) {
	static auto states = std::map<
		not_null<Main::Session*>,
		std::unique_ptr<ExternalTranscriptions>>();
	if (const auto i = states.find(session); i != end(states)) {
		return *i->second;
	}
	const auto i = states.emplace(
		session,
		std::make_unique<ExternalTranscriptions>(session)).first;
	session->lifetime().add([=] { states.erase(session); });
	return *i->second;
}

const Api::Transcribes::Entry *ExternalTranscriptions::find(
		not_null<HistoryItem*> item) const {
	const auto i = _entries.find(item->fullId());
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	return (i != end(_entries)
		&& _selected
		&& document
		&& !media->ttlSeconds()
		&& document->id == i->second.documentId)
		? &i->second.value
		: nullptr;
}

bool ExternalTranscriptions::toggle(not_null<HistoryItem*> item) {
	if (!find(item)) {
		return false;
	}
	auto &cached = _entries[item->fullId()];
	cached.value.shown = !cached.value.shown;
	cached.accessed = ++_accessed;
	refresh(item->fullId());
	return true;
}

bool ExternalTranscriptions::set(
		not_null<HistoryItem*> item,
		DocumentId documentId,
		const QByteArray &serviceConfig,
		uint64 generation,
		QString result) {
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	if (&item->history()->session() != _session
		|| !_selected
		|| !document
		|| document->id != documentId
		|| (!document->isVoiceMessage() && !document->isVideoMessage())
		|| media->ttlSeconds()
		|| serviceConfig != _config
		|| generation != this->generation(document->isVideoMessage())
		|| result.isEmpty()
		|| result.size() > kMaximumText
		|| result.contains(QChar(0))
		|| QString::fromUtf8(result.toUtf8()) != result) {
		return false;
	}
	_entries[item->fullId()] = {
		.documentId = documentId,
		.value = {
			.result = std::move(result),
			.shown = true,
			.roundview = document->isVideoMessage(),
		},
		.accessed = ++_accessed,
	};
	if (_entries.size() > kMaximumEntries) {
		const auto oldest = ranges::min_element(
			_entries,
			ranges::less(),
			[](const auto &entry) { return entry.second.accessed; });
		const auto id = oldest->first;
		_entries.erase(oldest);
		refresh(id);
	}
	refresh(item->fullId());
	return true;
}

void ExternalTranscriptions::refresh(FullMsgId id) {
	if (const auto item = _session->data().message(id)) {
		_session->data().requestItemViewRefresh(item);
		_session->data().requestItemResize(item);
	}
}

void ExternalTranscriptions::clear() {
	for (auto &generation : _generation) {
		++generation;
	}
	auto removed = std::vector<FullMsgId>();
	for (const auto &[id, cached] : _entries) {
		removed.push_back(id);
	}
	_entries.clear();
	for (const auto &id : removed) {
		refresh(id);
	}
}

[[nodiscard]] DocumentData *AudioDocument(HistoryItem *item) {
	const auto media = item ? item->media() : nullptr;
	const auto document = media ? media->document() : nullptr;
	return (document
		&& (document->isVoiceMessage() || document->isVideoMessage()))
		? document
		: nullptr;
}

[[nodiscard]] QByteArray ReadAudio(
		not_null<DocumentData*> document,
		const std::shared_ptr<Data::DocumentMedia> &media) {
	auto bytes = media->bytes();
	if (bytes.isEmpty()) {
		const auto location = document->location(true);
		if (!location.isEmpty() && location.accessEnable()) {
			auto file = QFile(location.name());
			if (file.open(QIODevice::ReadOnly)) {
				bytes = file.read(kTranscribeMaxBytes + 1);
			}
			location.accessDisable();
		}
	}
	return bytes;
}

AudioDownload::AudioDownload() {
	_deadline.setSingleShot(true);
	QObject::connect(&_deadline, &QTimer::timeout, &_deadline, [=] {
		fail(tr::lng_nagram_transcribe_download_error(tr::now));
	});
}

AudioDownload::~AudioDownload() {
	cancel();
}

void AudioDownload::cancel() {
	_lifetime.destroy();
	_deadline.stop();
	_done = nullptr;
	_failed = nullptr;
	if (_ownsDownload && _session) {
		const auto item = _session->data().message(_id);
		const auto document = AudioDocument(item);
		if (document && document->id == _documentId && document->loading()) {
			document->cancel();
		}
	}
	_ownsDownload = false;
	_media.reset();
}

void AudioDownload::fail(QString error) {
	const auto failed = std::move(_failed);
	cancel();
	if (failed) {
		failed(std::move(error));
	}
}

void AudioDownload::start(
		not_null<Main::Session*> session,
		FullMsgId id,
		DocumentId documentId,
		std::shared_ptr<Data::DocumentMedia> media,
		Fn<void(QByteArray)> done,
		Fn<void(QString)> failed) {
	cancel();
	_session = base::make_weak(session);
	_id = id;
	_documentId = documentId;
	_media = std::move(media);
	_done = std::move(done);
	_failed = std::move(failed);
	const auto item = session->data().message(id);
	const auto document = AudioDocument(item);
	if (!document || document->id != documentId || item->media()->ttlSeconds()) {
		fail(tr::lng_nagram_transcribe_missing(tr::now));
		return;
	} else if (document->size <= 0 || document->size > kTranscribeMaxBytes) {
		fail(ServiceErrorText(ServiceError::TooLarge));
		return;
	}
	if (!ReadAudio(document, _media).isEmpty()) {
		check();
		return;
	}
	_deadline.start(kDownloadTimeout);
	session->data().documentLoadProgress() | rpl::on_next([=](
			not_null<DocumentData*> updated) {
		if (updated->id == _documentId) {
			check();
		}
	}, _lifetime);
	_ownsDownload = !document->loading();
	document->save(id, QString());
	check();
}

void AudioDownload::check() {
	if (!_done || !_session) {
		return;
	}
	const auto item = _session->data().message(_id);
	const auto document = AudioDocument(item);
	if (!document || document->id != _documentId || item->media()->ttlSeconds()) {
		fail(tr::lng_nagram_transcribe_missing(tr::now));
		return;
	}
	if (document->loading() && !_media->loaded()) {
		return;
	}
	auto bytes = ReadAudio(document, _media);
	if (!bytes.isEmpty()) {
		const auto done = std::move(_done);
		_ownsDownload = false;
		cancel();
		done(std::move(bytes));
	} else if (!document->loading()) {
		fail(tr::lng_nagram_transcribe_download_error(tr::now));
	}
}

[[nodiscard]] std::optional<QString> TranscriptText(const QByteArray &body) {
	const auto value = QJsonDocument::fromJson(body).object().value(
		u"text"_q);
	const auto text = value.toString();
	return (value.isString()
		&& !text.isEmpty()
		&& text.size() <= kMaximumText
		&& !text.contains(QChar(0)))
		? std::make_optional(text)
		: std::nullopt;
}

struct BatchEntry {
	FullMsgId id;
	DocumentId documentId = 0;
	std::shared_ptr<Data::DocumentMedia> media;
};

struct BatchScope {
	std::vector<BatchEntry> entries;
	int skipped = 0;
};

[[nodiscard]] BatchScope CollectBatch(
		not_null<Main::Session*> session,
		const MessageIdsList &ids) {
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto &id : ids) {
		if (const auto item = session->data().message(id)) {
			items.push_back(item);
		}
	}
	ranges::sort(items, [](const auto &a, const auto &b) {
		return (a->date() != b->date())
			? (a->date() < b->date())
			: (a->fullId() < b->fullId());
	});
	const auto &cache = ExternalTranscriptions::For(session);
	auto candidates = std::vector<TranscribeCandidate>();
	auto entries = std::vector<BatchEntry>();
	for (const auto &item : items) {
		const auto document = AudioDocument(item);
		auto media = document ? document->createMediaView() : nullptr;
		candidates.push_back({
			.audio = (document != nullptr),
			.expiring = document && (item->media()->ttlSeconds() != 0),
			.cached = document && (cache.find(item) != nullptr),
			.downloaded = media && media->loaded(true),
			.size = document ? document->size : 0,
		});
		entries.push_back({
			.id = item->fullId(),
			.documentId = document ? document->id : DocumentId(),
			.media = std::move(media),
		});
	}
	const auto plan = PlanTranscription(candidates);
	auto result = BatchScope{ .skipped = plan.skipped };
	for (const auto &index : plan.accepted) {
		result.entries.push_back(std::move(entries[index]));
	}
	return result;
}

void BatchTranscriptionBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		MessageIdsList ids,
		ServiceDefinition service,
		QByteArray config) {
	struct State {
		ServiceRequest request;
		AudioDownload download;
		BatchScope scope;
		TranscriptionQueue queue;
		ServiceError error = ServiceError::None;
		int status = 0;
		bool started = false;
		Fn<void()> step;
	};
	session->lifetime().add(crl::guard(box, [=] { box->closeBox(); }));
	box->setTitle(tr::lng_nagram_menu_transcribe_selected());
	const auto state = box->lifetime().make_state<State>();
	state->scope = CollectBatch(session, ids);
	const auto total = int(state->scope.entries.size());
	const auto skipped = state->scope.skipped;
	const auto skippedText = skipped
		? (u"\n\n"_q + tr::lng_nagram_transcribe_batch_skipped(
			tr::now,
			lt_amount,
			QString::number(skipped),
			lt_limit,
			QString::number(kTranscribeBatchLimit)))
		: QString();
	if (!total) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_transcribe_batch_empty(tr::now) + skippedText,
			st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_transcribe_batch_about(
			tr::now,
			lt_amount,
			QString::number(total),
			lt_name,
			service.name,
			lt_url,
			ServiceEndpoint(service).toDisplayString()) + skippedText,
		st::boxLabel));
	const auto label = box->addRow(
		object_ptr<Ui::FlatLabel>(box, st::boxLabel));
	const auto finish = [=] {
		const auto &queue = state->queue;
		const auto summary = tr::lng_nagram_transcribe_batch_done(
			tr::now,
			lt_done,
			QString::number(queue.done()),
			lt_failed,
			QString::number(queue.failed()),
			lt_skipped,
			QString::number(skipped + queue.left()));
		const auto reason = (queue.stopped() == TranscribeStop::Changed)
			? tr::lng_nagram_config_changed_error(tr::now)
			: (queue.stopped() == TranscribeStop::Finished)
			? QString()
			: ServiceErrorText(state->error, state->status);
		label->setText(reason.isEmpty()
			? summary
			: (tr::lng_nagram_transcribe_batch_stopped(
				tr::now,
				lt_reason,
				reason) + u"\n\n"_q + summary));
	};
	state->step = crl::guard(box, [=] {
		const auto index = state->queue.current();
		if (!index) {
			finish();
			return;
		} else if (ForDevice().Get(kServicesConfig) != config) {
			state->queue.report(TranscribeOutcome::Changed);
			state->step();
			return;
		}
		const auto &entry = state->scope.entries[*index];
		const auto item = session->data().message(entry.id);
		const auto document = AudioDocument(item);
		if (!document || document->id != entry.documentId || item->media()->ttlSeconds()) {
			state->queue.report(TranscribeOutcome::Failed);
			state->step();
			return;
		}
		const auto round = document->isVideoMessage();
		const auto generation = ExternalTranscriptions::For(session).generation(round);
		label->setText(tr::lng_nagram_transcribe_download(tr::now));
		state->download.start(
			session,
			entry.id,
			entry.documentId,
			entry.media,
			crl::guard(box, [=](QByteArray bytes) {
				if (ForDevice().Get(kServicesConfig) != config
					|| generation != ExternalTranscriptions::For(session).generation(round)) {
					state->queue.report(TranscribeOutcome::Changed);
					state->step();
					return;
				}
				label->setText(tr::lng_nagram_transcribe_batch_progress(
					tr::now,
					lt_index,
					QString::number(*index + 1),
					lt_amount,
					QString::number(total)));
				state->request.audio(
					service,
					std::move(bytes),
					round ? u"audio.mp4"_q : u"audio.ogg"_q,
					crl::guard(box, [=](ServiceResult response) {
						const auto text = (response.error == ServiceError::None)
							? TranscriptText(response.body)
							: std::nullopt;
						const auto current = session->data().message(entry.id);
						const auto stored = text
							&& current
							&& ExternalTranscriptions::For(session).set(
								current,
								entry.documentId,
								config,
								generation,
								*text);
						if (!stored) {
							state->error = (response.error == ServiceError::None)
								? ServiceError::Response
								: response.error;
							state->status = response.status;
						}
						state->queue.report(stored
							? TranscribeOutcome::Done
							: (response.error == ServiceError::Credential
								|| response.error == ServiceError::Configuration)
							? TranscribeOutcome::Fatal
							: (response.error == ServiceError::Network)
							? TranscribeOutcome::Network
							: TranscribeOutcome::Failed);
						state->step();
					}));
			}),
			crl::guard(box, [=](QString error) {
				state->error = ServiceError::Network;
				state->queue.report(TranscribeOutcome::Network);
				label->setText(std::move(error));
				state->step();
			}));
	});
	box->addButton(tr::lng_nagram_transcribe_batch_start(), [=] {
		if (state->started) {
			return;
		}
		state->started = true;
		state->queue = TranscriptionQueue(total);
		state->step();
	});
	box->addButton(tr::lng_cancel(), [=] {
		state->queue.cancel();
		state->download.cancel();
		state->request.cancel();
		box->closeBox();
	});
}

} // namespace

void InsertTranscribeSelectedAction(
		Ui::PopupMenu *menu,
		Window::SessionController *controller,
		MessageIdsList selected) {
	if (!menu || !controller || selected.empty()) {
		return;
	}
	const auto session = &controller->session();
	if (!ExternalTranscriptions::For(session).selected()
		|| ranges::none_of(selected, [&](const FullMsgId &id) {
			const auto item = session->data().message(id);
			return AudioDocument(item) && !item->media()->ttlSeconds();
		})) {
		return;
	}
	const auto action = Ui::Menu::CreateAction(
		menu,
		tr::lng_nagram_menu_transcribe_selected(tr::now),
		crl::guard(controller, [=] {
			const auto config = Services();
			const auto service = config
				? FindService(
					*config,
					config->value(u"transcription"_q).toString())
				: std::nullopt;
			if (!service || service->kind != ServiceKind::Transcription) {
				controller->showToast(tr::lng_nagram_service_invalid(tr::now));
				return;
			}
			controller->show(Box(
				BatchTranscriptionBox,
				session,
				selected,
				*service,
				ForDevice().Get(kServicesConfig)));
		}));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(),
		menu->menu()->st(),
		action,
		&st::menuIconSoundOn,
		&st::menuIconSoundOn);
	Menu::Tag(
		menu->insertAction(Menu::EndPosition(menu), std::move(widget)),
		Menu::ActionId::TranscribeSelected);
}

bool ExternalTranscriptionSelected(not_null<Main::Session*> session) {
	return ExternalTranscriptions::For(session).selected();
}

const Api::Transcribes::Entry *TranscriptionOverride(
		not_null<HistoryItem*> item) {
	const auto &external = ExternalTranscriptions::For(
		&item->history()->session());
	if (!external.selected()) {
		return nullptr;
	}
	if (const auto found = external.find(item)) {
		return found;
	}
	static const auto empty = Api::Transcribes::Entry();
	return &empty;
}

void ShowCustomTranscription(
		std::shared_ptr<Main::SessionShow> show,
		not_null<HistoryItem*> item,
		bool manage) {
	const auto session = &show->session();
	if (!manage && ExternalTranscriptions::For(session).toggle(item)) {
		return;
	}
	const auto config = Services();
	const auto service = config
		? FindService(*config, config->value(u"transcription"_q).toString())
		: std::nullopt;
	const auto document = item->media() ? item->media()->document() : nullptr;
	if (!service || service->kind != ServiceKind::Transcription || !document) {
		show->showToast(tr::lng_nagram_service_invalid(tr::now));
		return;
	}
	const auto id = item->fullId();
	const auto documentId = document->id;
	const auto serviceConfig = ForDevice().Get(kServicesConfig);
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {
		const auto current = show->session().data().message(id);
		const auto currentMedia = current ? current->media() : nullptr;
		const auto currentDocument = currentMedia
			? currentMedia->document() : nullptr;
		if (!currentDocument || currentDocument->id != documentId
			|| currentMedia->ttlSeconds()) {
			box->addRow(object_ptr<Ui::FlatLabel>(box,
				tr::lng_nagram_transcribe_missing(), st::boxLabel));
			return;
		}
		struct State {
			ServiceRequest request;
			AudioDownload download;
			std::shared_ptr<Data::DocumentMedia> media;
			QString result;
			bool loading = false;
		};
		session->lifetime().add(crl::guard(box, [=] { box->closeBox(); }));
		box->setTitle(tr::lng_nagram_service_transcription());
		const auto state = box->lifetime().make_state<State>();
		state->media = currentDocument->createMediaView();
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_transcribe_upload_about(
				lt_name, rpl::single(service->name),
				lt_url, rpl::single(ServiceEndpoint(*service).toDisplayString())),
			st::boxLabel));
		const auto label = box->addRow(object_ptr<Ui::FlatLabel>(box, st::boxLabel));
		label->setSelectable(current->allowsForward()
			|| Privacy::ForceCopy());
		state->result = show->session().api().transcribes().entry(current).result;
		label->setText(state->result);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_transcribe_cache_about(),
			st::boxLabel));
		box->addButton(tr::lng_nagram_transcribe_start(), [=] {
			if (state->loading) {
				return;
			}
			if (ForDevice().Get(kServicesConfig)
				!= serviceConfig) {
				label->setText(tr::lng_nagram_service_invalid(tr::now));
				return;
			}
			const auto item = show->session().data().message(id);
			const auto media = item ? item->media() : nullptr;
			const auto document = media ? media->document() : nullptr;
			if (!document || document->id != documentId) {
				box->showToast(tr::lng_nagram_transcribe_missing(tr::now));
				return;
			}
			if (media->ttlSeconds()) {
				box->showToast(tr::lng_nagram_transcribe_missing(tr::now));
				return;
			}
			if (document->size > kTranscribeMaxBytes) {
				label->setText(ServiceErrorText(ServiceError::TooLarge));
				return;
			}
			state->loading = true;
			label->setText(tr::lng_nagram_transcribe_download(tr::now));
			const auto round = document->isVideoMessage();
			const auto generation = ExternalTranscriptions::For(session).generation(round);
			state->download.start(
				session,
				id,
				documentId,
				state->media,
				crl::guard(box, [=](QByteArray bytes) {
					if (ForDevice().Get(kServicesConfig) != serviceConfig
						|| generation != ExternalTranscriptions::For(session).generation(round)) {
						state->loading = false;
						label->setText(tr::lng_nagram_config_changed_error(tr::now));
						return;
					}
					label->setText(tr::lng_contacts_loading(tr::now));
					state->request.audio(*service, std::move(bytes),
						round ? u"audio.mp4"_q : u"audio.ogg"_q,
						crl::guard(box, [=](ServiceResult response) {
							state->loading = false;
							if (response.error != ServiceError::None) {
								label->setText(ServiceErrorText(response.error, response.status));
								return;
							}
							const auto text = TranscriptText(response.body);
							if (!text) {
								label->setText(ServiceErrorText(ServiceError::Response));
								return;
							}
							const auto item = show->session().data().message(id);
							if (!item || !ExternalTranscriptions::For(session).set(
									item, documentId, serviceConfig, generation, *text)) {
								label->setText(tr::lng_nagram_transcribe_missing(tr::now));
								return;
							}
							state->result = *text;
							label->setText(state->result);
						}));
				}),
				crl::guard(box, [=](QString error) {
					state->loading = false;
					label->setText(std::move(error));
				}));
		});
		box->addButton(tr::lng_context_copy_text(), [=] {
			const auto item = show->session().data().message(id);
			const auto media = item ? item->media() : nullptr;
			const auto document = media ? media->document() : nullptr;
			if (item && (item->allowsForward() || Privacy::ForceCopy())
				&& document
				&& document->id == documentId && !media->ttlSeconds()
				&& !state->result.isEmpty()
				&& show->session().api().transcribes().entry(item).result
					== state->result) {
				TextUtilities::SetClipboardText(TextForMimeData::Simple(state->result));
			}
		});
		box->addButton(tr::lng_cancel(), [=] {
			state->download.cancel();
			state->request.cancel();
			box->closeBox();
		});
	}));
}

} // namespace Nagram
