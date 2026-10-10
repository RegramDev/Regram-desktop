#pragma once

#include "regram/nagram/media/sticker_export_model.h"
#include "base/timer.h"
#include "base/weak_ptr.h"

class DocumentData;

namespace Data {
class DocumentMedia;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Nagram::Media {

class StickerExport final : public base::has_weak_ptr {
public:
	explicit StickerExport(not_null<Main::Session*> session);
	~StickerExport();

	static void Attach(not_null<Main::Session*> session);
	[[nodiscard]] static StickerExport *Find(
		not_null<Main::Session*> session);

	void syncNow();
	[[nodiscard]] rpl::producer<ExportStatus> statusValue() const;

private:
	struct Job {
		QString dir;
		ExportSetManifest manifest;
		ExportManifestSet entry;
		std::vector<not_null<DocumentData*>> documents;
		int next = 0;
		int loading = 0;
		bool failed = false;
	};
	struct Loading {
		quint64 setId = 0;
		QString file;
		std::shared_ptr<Data::DocumentMedia> media;
	};

	void optionsChanged(const QString &path);
	void stickersUpdated();
	void scheduleAuto();
	void start(bool manual);
	void waitTimedOut();
	void reset();
	void publish();

	[[nodiscard]] QString resolveDir(const ExportPlanSet &set) const;
	[[nodiscard]] bool prepareJob(const ExportPlanSet &set, quint64 hash);
	void pump();
	void startNext(quint64 setId, Job &job);
	void loadProgress(not_null<DocumentData*> document);
	bool store(const QString &file, const QByteArray &bytes);
	void checkJob(quint64 setId);
	[[nodiscard]] bool finishJob(quint64 setId, Job &job);
	void fail(ExportError error);

	const not_null<Main::Session*> _session;
	ExportRun _run;
	rpl::variable<ExportStatus> _status;
	int _generation = 0;
	bool _manual = false;
	bool _listRequested = false;
	bool _pumpQueued = false;
	QString _root;

	std::map<quint64, Job> _jobs;
	std::map<not_null<DocumentData*>, Loading> _loading;
	std::set<quint64> _waiting;

	base::Timer _autoTimer;
	base::Timer _restartTimer;
	base::Timer _waitTimer;

	rpl::lifetime _updatesLifetime;
	rpl::lifetime _lifetime;

};

} // namespace Nagram::Media
