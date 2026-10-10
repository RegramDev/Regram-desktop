#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>
#include <set>
#include <vector>

namespace Nagram::Media {

inline constexpr auto kExportDirNamingCount = 3;
inline constexpr auto kMaxExportPathLength = 1024;
inline constexpr auto kMaxExportDirLength = 64;

enum class ExportDirNaming { ShortName, Title, Id };
enum class ExportError { None, Path, Write };

struct ExportSetInfo {
	quint64 id = 0;
	quint64 hash = 0;
	int count = 0;
	QString shortName;
	QString title;
};

struct ExportManifestSet {
	quint64 id = 0;
	QString dir;
	quint64 hash = 0;
	int count = 0;

	friend bool operator==(
		const ExportManifestSet &,
		const ExportManifestSet &) = default;
};

struct ExportManifest {
	std::vector<ExportManifestSet> sets;

	friend bool operator==(
		const ExportManifest &,
		const ExportManifest &) = default;
};

struct ExportSticker {
	quint64 id = 0;
	QString file;
	QString emoji;

	friend bool operator==(
		const ExportSticker &,
		const ExportSticker &) = default;
};

struct ExportSetManifest {
	quint64 id = 0;
	QString shortName;
	QString title;
	std::vector<ExportSticker> stickers;

	friend bool operator==(
		const ExportSetManifest &,
		const ExportSetManifest &) = default;
};

struct ExportPlanSet {
	quint64 id = 0;
	QString dir;
	QString renameFrom;
	bool write = false;

	friend bool operator==(
		const ExportPlanSet &,
		const ExportPlanSet &) = default;
};

struct ExportPlan {
	std::vector<ExportPlanSet> sets;
	std::vector<quint64> removed;

	[[nodiscard]] bool empty() const {
		return sets.empty() && removed.empty();
	}
};

struct ExportStatus {
	bool running = false;
	bool finished = false;
	int done = 0;
	int total = 0;
	int failed = 0;
	ExportError error = ExportError::None;

	friend bool operator==(
		const ExportStatus &,
		const ExportStatus &) = default;
};

[[nodiscard]] constexpr bool ValidExportDirNaming(const int &value) {
	return value >= 0 && value < kExportDirNamingCount;
}
[[nodiscard]] bool ValidExportPath(const QString &value);

[[nodiscard]] QString SanitizeExportDirName(const QString &name);
[[nodiscard]] bool SafeExportDirName(const QString &name);
[[nodiscard]] QString ExportDirWithId(const QString &name, quint64 id);
[[nodiscard]] std::vector<QString> ExportDirNames(
	const std::vector<ExportSetInfo> &sets,
	int naming);

[[nodiscard]] QString ExportFileName(quint64 id, const QString &extension);
[[nodiscard]] bool SafeExportFileName(const QString &name);

[[nodiscard]] std::optional<ExportManifest> ParseExportManifest(
	const QByteArray &bytes);
[[nodiscard]] QByteArray SerializeExportManifest(
	const ExportManifest &manifest);
[[nodiscard]] std::optional<ExportSetManifest> ParseExportSetManifest(
	const QByteArray &bytes);
[[nodiscard]] QByteArray SerializeExportSetManifest(
	const ExportSetManifest &manifest);

[[nodiscard]] ExportPlan PlanExport(
	const std::optional<ExportManifest> &previous,
	const std::vector<ExportSetInfo> &current,
	int naming);
[[nodiscard]] QStringList StaleExportFiles(
	const std::optional<ExportSetManifest> &previous,
	const ExportSetManifest &next);
void UpsertExportSet(ExportManifest &manifest, ExportManifestSet set);
void RemoveExportSets(ExportManifest &manifest, const std::vector<quint64> &ids);

class ExportRun final {
public:
	[[nodiscard]] int begin(bool manual);
	[[nodiscard]] bool plan(int generation, const std::vector<quint64> &sets);
	void invalidate();

	[[nodiscard]] bool accepts(int generation) const;
	[[nodiscard]] bool setDone(int generation, quint64 id);
	[[nodiscard]] bool setFailed(int generation, quint64 id);
	[[nodiscard]] bool fail(int generation, ExportError error);

	[[nodiscard]] bool autoSyncPaused() const;
	[[nodiscard]] const ExportStatus &status() const;

private:
	[[nodiscard]] bool resolve(int generation, quint64 id, bool failed);
	void checkFinished();

	int _generation = 0;
	bool _manual = false;
	bool _paused = false;
	std::set<quint64> _pending;
	ExportStatus _status;

};

} // namespace Nagram::Media
