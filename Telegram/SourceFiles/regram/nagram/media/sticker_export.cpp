#include "regram/nagram/media/sticker_export.h"

#include "regram/nagram/media/backend_options.h"
#include "apiwrap.h"
#include "core/application.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "window/window_controller.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>

namespace Nagram::Media {
namespace {

constexpr auto kAutoSyncDelay = crl::time(3000);
constexpr auto kRestartDelay = crl::time(1000);
constexpr auto kLoadTimeout = crl::time(30000);
constexpr auto kMaxLoads = 4;

[[nodiscard]] std::map<Main::Session*, StickerExport*> &Instances() {
	static auto result = std::map<Main::Session*, StickerExport*>();
	return result;
}

[[nodiscard]] QString ManifestPath(const QString &root) {
	return QDir(root).filePath(u"regram-stickers.json"_q);
}

[[nodiscard]] QString SetManifestPath(const QString &root, const QString &dir) {
	return QDir(QDir(root).filePath(dir)).filePath(u"set.json"_q);
}

[[nodiscard]] QByteArray ReadFile(const QString &path) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

[[nodiscard]] bool WriteFile(const QString &path, const QByteArray &bytes) {
	auto file = QSaveFile(path);
	return file.open(QIODevice::WriteOnly)
		&& (file.write(bytes) == bytes.size())
		&& file.commit();
}

[[nodiscard]] std::optional<ExportManifest> ReadManifest(const QString &root) {
	auto path = ManifestPath(root);
	if (!QFileInfo::exists(path)) {
		path = QDir(root).filePath(u"nagram-stickers.json"_q);
	}
	return QFileInfo::exists(path)
		? ParseExportManifest(ReadFile(path))
		: std::make_optional(ExportManifest());
}

[[nodiscard]] QString Extension(not_null<DocumentData*> document) {
	const auto info = document->sticker();
	return !info
		? QString()
		: info->isLottie()
		? u"tgs"_q
		: info->isWebm()
		? u"webm"_q
		: u"webp"_q;
}

[[nodiscard]] QByteArray ReadyBytes(
		not_null<DocumentData*> document,
		const std::shared_ptr<Data::DocumentMedia> &media) {
	if (const auto bytes = media->bytes(); !bytes.isEmpty()) {
		return bytes;
	}
	const auto path = document->filepath(true);
	return path.isEmpty() ? QByteArray() : ReadFile(path);
}

void ShowError(const QString &text) {
	if (const auto window = Core::App().activeWindow()) {
		window->showToast(text);
	}
}

} // namespace

StickerExport::StickerExport(not_null<Main::Session*> session)
: _session(session)
, _autoTimer([=] { start(false); })
, _restartTimer([=] { start(_manual); })
, _waitTimer([=] { waitTimedOut(); }) {
	Instances().emplace(session.get(), this);

	rpl::combine(
		ForDevice().Value(kStickerExportPath),
		ForDevice().Value(kStickerExportDirNaming)
	) | rpl::on_next([=](const QString &path, int) {
		optionsChanged(path);
	}, _lifetime);

	ForDevice().Value(
		kStickerExportAutoSync
	) | rpl::on_next([=] {
		scheduleAuto();
	}, _lifetime);

	_session->data().documentLoadProgress(
	) | rpl::on_next([=](not_null<DocumentData*> document) {
		loadProgress(document);
	}, _lifetime);
}

StickerExport::~StickerExport() {
	Instances().erase(_session.get());
}

void StickerExport::Attach(not_null<Main::Session*> session) {
	session->lifetime().make_state<StickerExport>(session);
}

StickerExport *StickerExport::Find(not_null<Main::Session*> session) {
	const auto i = Instances().find(session.get());
	return (i != Instances().end()) ? i->second : nullptr;
}

rpl::producer<ExportStatus> StickerExport::statusValue() const {
	return _status.value();
}

void StickerExport::syncNow() {
	start(true);
}

void StickerExport::optionsChanged(const QString &path) {
	reset();
	_updatesLifetime.destroy();
	if (path.isEmpty()) {
		_autoTimer.cancel();
		return;
	}
	_session->data().stickers().updated(
		Data::StickersType::Stickers
	) | rpl::on_next([=] {
		stickersUpdated();
	}, _updatesLifetime);
	scheduleAuto();
}

void StickerExport::stickersUpdated() {
	if (_run.status().running) {
		_restartTimer.callOnce(kRestartDelay);
	} else {
		scheduleAuto();
	}
}

void StickerExport::scheduleAuto() {
	if (ForDevice().Get(kStickerExportAutoSync)
		&& !ForDevice().Get(kStickerExportPath).isEmpty()
		&& !_run.autoSyncPaused()
		&& !_run.status().running) {
		_autoTimer.callOnce(kAutoSyncDelay);
	}
}

void StickerExport::reset() {
	_run.invalidate();
	_jobs.clear();
	_loading.clear();
	_waiting.clear();
	_restartTimer.cancel();
	_waitTimer.cancel();
	publish();
}

void StickerExport::publish() {
	_status = _run.status();
}

void StickerExport::fail(ExportError error) {
	if (!_run.fail(_generation, error)) {
		return;
	}
	_jobs.clear();
	_loading.clear();
	_waiting.clear();
	_restartTimer.cancel();
	_waitTimer.cancel();
	publish();
	LOG(("Nagram Sticker Export: stopped, the folder %1."
		).arg((error == ExportError::Path)
			? u"is missing or not writable"_q
			: u"could not be written to"_q));
	ShowError((error == ExportError::Path)
		? tr::lng_nagram_sticker_export_error_path(tr::now)
		: tr::lng_nagram_sticker_export_error_write(tr::now));
}

void StickerExport::waitTimedOut() {
	if (!_run.accepts(_generation)) {
		return;
	} else if (!_run.status().total) {
		start(_manual);
		return;
	}
	for (const auto id : base::take(_waiting)) {
		if (_run.setFailed(_generation, id)) {
			publish();
		}
	}
}

void StickerExport::start(bool manual) {
	reset();
	_autoTimer.cancel();
	_root = ForDevice().Get(kStickerExportPath);
	if (_root.isEmpty()) {
		return;
	}
	_manual = manual;
	_generation = _run.begin(manual);
	publish();

	const auto info = QFileInfo(_root);
	if (!info.isDir() || !info.isWritable()) {
		fail(ExportError::Path);
		return;
	}
	auto &stickers = _session->data().stickers();
	if (stickers.setsOrder().empty()
		&& stickers.updateNeeded(crl::now())
		&& !_listRequested) {
		_listRequested = true;
		_session->api().updateStickers();
		_waitTimer.callOnce(kLoadTimeout);
		return;
	}

	auto current = std::vector<ExportSetInfo>();
	auto loaded = std::set<quint64>();
	const auto &sets = stickers.sets();
	for (const auto id : stickers.setsOrder()) {
		const auto i = sets.find(id);
		if (i == sets.end()) {
			continue;
		}
		const auto set = i->second.get();
		using Flag = Data::StickersSetFlag;
		if (set->type() != Data::StickersType::Stickers
			|| !(set->flags & Flag::Installed)
			|| (set->flags & (Flag::Archived | Flag::Special))) {
			continue;
		}
		current.push_back({
			.id = set->id,
			.hash = set->hash,
			.count = set->count,
			.shortName = set->shortName,
			.title = set->title,
		});
		if (!(set->flags & Flag::NotLoaded) && !set->stickers.isEmpty()) {
			loaded.insert(set->id);
		}
	}

	auto previous = ReadManifest(_root);
	auto plan = PlanExport(
		previous,
		current,
		ForDevice().Get(kStickerExportDirNaming));
	if (previous) {
		for (const auto &old : previous->sets) {
			const auto planned = ranges::contains(
				plan.sets,
				old.id,
				&ExportPlanSet::id);
			const auto removed = ranges::contains(plan.removed, old.id);
			if (!planned
				&& !removed
				&& !QFileInfo::exists(SetManifestPath(_root, old.dir))) {
				plan.sets.push_back({ old.id, old.dir, QString(), true });
			}
		}
	}

	auto manifest = previous.value_or(ExportManifest());
	auto changed = !plan.removed.empty();
	RemoveExportSets(manifest, plan.removed);
	auto ids = std::vector<quint64>();
	for (auto &set : plan.sets) {
		const auto dir = resolveDir(set);
		const auto from = base::take(set.renameFrom);
		set.dir = dir;
		if (!from.isEmpty() && from != dir) {
			if (QDir(_root).rename(from, dir)) {
				for (auto &entry : manifest.sets) {
					if (entry.id == set.id) {
						entry.dir = dir;
						changed = true;
					}
				}
			} else {
				set.write = true;
			}
		}
		if (set.write) {
			ids.push_back(set.id);
		}
	}
	if (changed && !WriteFile(
			ManifestPath(_root),
			SerializeExportManifest(manifest))) {
		fail(ExportError::Write);
		return;
	}
	if (!_run.plan(_generation, ids)) {
		return;
	}
	for (const auto &set : plan.sets) {
		if (!set.write) {
			continue;
		} else if (!loaded.contains(set.id)) {
			const auto i = sets.find(set.id);
			_waiting.emplace(set.id);
			_session->api().scheduleStickerSetRequest(
				set.id,
				i->second->accessHash);
		} else if (!prepareJob(set, sets.find(set.id)->second->hash)) {
			return;
		}
	}
	if (!_waiting.empty()) {
		_session->api().requestStickerSets();
		_waitTimer.callOnce(kLoadTimeout);
	}
	publish();
	pump();
}

QString StickerExport::resolveDir(const ExportPlanSet &set) const {
	const auto existing = ParseExportSetManifest(
		ReadFile(SetManifestPath(_root, set.dir)));
	return (existing && existing->id != set.id)
		? ExportDirWithId(set.dir, set.id)
		: set.dir;
}

bool StickerExport::prepareJob(const ExportPlanSet &set, quint64 hash) {
	const auto &sets = _session->data().stickers().sets();
	const auto source = sets.find(set.id)->second.get();
	if (!QDir(_root).mkpath(set.dir)) {
		fail(ExportError::Write);
		return false;
	}
	auto job = Job{
		.dir = QDir(_root).filePath(set.dir),
		.manifest = {
			.id = source->id,
			.shortName = source->shortName,
			.title = source->title,
		},
		.entry = { source->id, set.dir, hash, source->count },
	};
	for (const auto &document : source->stickers) {
		const auto extension = Extension(document);
		if (extension.isEmpty()) {
			continue;
		}
		job.manifest.stickers.push_back({
			.id = document->id,
			.file = ExportFileName(document->id, extension),
			.emoji = document->sticker()->alt,
		});
		job.documents.push_back(document);
	}
	_jobs.emplace(set.id, std::move(job));
	return true;
}

void StickerExport::pump() {
	if (_pumpQueued || !_run.accepts(_generation)) {
		return;
	}
	auto ready = std::vector<quint64>();
	for (const auto &[setId, job] : _jobs) {
		const auto dispatched = (job.next == int(job.documents.size()));
		if (!job.loading && (job.failed || dispatched)) {
			ready.push_back(setId);
		}
	}
	for (const auto setId : ready) {
		checkJob(setId);
		if (!_run.accepts(_generation)) {
			return;
		}
	}
	if (int(_loading.size()) >= kMaxLoads) {
		return;
	}
	for (auto &[setId, job] : _jobs) {
		if (job.failed || job.next == int(job.documents.size())) {
			continue;
		}
		_pumpQueued = true;
		crl::on_main(this, [=] {
			_pumpQueued = false;
			pump();
		});
		startNext(setId, job);
		return;
	}
}

void StickerExport::startNext(quint64 setId, Job &job) {
	const auto index = job.next++;
	const auto document = job.documents[index];
	const auto file = QDir(job.dir).filePath(
		job.manifest.stickers[index].file);
	if (QFileInfo(file).size() == document->size && document->size > 0) {
		return;
	}
	const auto media = document->createMediaView();
	if (const auto bytes = ReadyBytes(document, media); !bytes.isEmpty()) {
		store(file, bytes);
		return;
	} else if (_loading.contains(document)) {
		job.failed = true;
		return;
	}
	++job.loading;
	_loading.emplace(document, Loading{ setId, file, media });
	document->save(document->stickerSetOrigin(), QString());
	if (!document->loading()) {
		loadProgress(document);
	}
}

void StickerExport::loadProgress(not_null<DocumentData*> document) {
	const auto i = _loading.find(document);
	if (i == _loading.end() || document->loading()) {
		return;
	}
	const auto loading = base::take(i->second);
	_loading.erase(i);
	const auto job = _jobs.find(loading.setId);
	if (job == _jobs.end() || !_run.accepts(_generation)) {
		return;
	}
	--job->second.loading;
	const auto bytes = ReadyBytes(document, loading.media);
	if (bytes.isEmpty()) {
		job->second.failed = true;
	} else if (!store(loading.file, bytes)) {
		return;
	}
	pump();
}

bool StickerExport::store(const QString &file, const QByteArray &bytes) {
	if (!_run.accepts(_generation) || WriteFile(file, bytes)) {
		return true;
	}
	fail(ExportError::Write);
	return false;
}

void StickerExport::checkJob(quint64 setId) {
	const auto i = _jobs.find(setId);
	if (i == _jobs.end() || i->second.loading) {
		return;
	}
	auto &job = i->second;
	const auto done = !job.failed
		&& (job.next == int(job.documents.size()));
	if (!job.failed && !done) {
		return;
	} else if (done && !finishJob(setId, job)) {
		return;
	}
	_jobs.erase(i);
	const auto counted = done
		? _run.setDone(_generation, setId)
		: _run.setFailed(_generation, setId);
	if (counted) {
		publish();
	}
}

bool StickerExport::finishJob(quint64 setId, Job &job) {
	const auto path = QDir(job.dir).filePath(u"set.json"_q);
	const auto stale = StaleExportFiles(
		ParseExportSetManifest(ReadFile(path)),
		job.manifest);
	auto manifest = ReadManifest(_root).value_or(ExportManifest());
	UpsertExportSet(manifest, job.entry);
	if (!WriteFile(path, SerializeExportSetManifest(job.manifest))
		|| !WriteFile(
			ManifestPath(_root),
			SerializeExportManifest(manifest))) {
		fail(ExportError::Write);
		return false;
	}
	for (const auto &name : stale) {
		QFile::remove(QDir(job.dir).filePath(name));
	}
	return true;
}

} // namespace Nagram::Media
