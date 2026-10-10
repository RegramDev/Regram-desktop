#include "regram/nagram/media/sticker_export_model.h"

#include "regram/nagram/core/owned_json.h"
#include "base/basic_types.h"

#include <QtCore/QDir>
#include <QtCore/QJsonArray>

#include <algorithm>
#include <map>

namespace Nagram::Media {
namespace {

constexpr auto kManifestVersion = 1;

[[nodiscard]] bool ForbiddenInName(QChar ch) {
	const auto code = ch.unicode();
	return (code < 0x20)
		|| (code >= 0x7F && code <= 0x9F)
		|| u"/\\<>:\"|?*"_q.contains(ch);
}

[[nodiscard]] QString TrimName(QString name) {
	while (!name.isEmpty()
		&& (name.back().isSpace() || name.back() == u'.')) {
		name.chop(1);
	}
	while (!name.isEmpty() && name.front().isSpace()) {
		name.remove(0, 1);
	}
	return name;
}

[[nodiscard]] bool ReservedName(const QString &name) {
	const auto base = name.left(name.indexOf(u'.')).trimmed().toUpper();
	if (base == u"CON"_q
		|| base == u"PRN"_q
		|| base == u"AUX"_q
		|| base == u"NUL"_q) {
		return true;
	}
	return base.size() == 4
		&& (base.startsWith(u"COM"_q) || base.startsWith(u"LPT"_q))
		&& base[3] >= u'1'
		&& base[3] <= u'9';
}

[[nodiscard]] QString BaseName(const ExportSetInfo &set, int naming) {
	switch (static_cast<ExportDirNaming>(naming)) {
	case ExportDirNaming::Title: return SanitizeExportDirName(set.title);
	case ExportDirNaming::Id: return QString::number(set.id);
	case ExportDirNaming::ShortName: break;
	}
	return SanitizeExportDirName(set.shortName);
}

[[nodiscard]] std::optional<QJsonObject> ParseVersioned(
		const QByteArray &bytes,
		QStringList keys) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	const auto object = document.object();
	const auto version = object.value(u"version"_q);
	keys.sort();
	return (object.keys() == keys
		&& version.isDouble()
		&& version.toDouble() == double(kManifestVersion))
		? std::make_optional(object)
		: std::nullopt;
}

[[nodiscard]] bool HasKeys(const QJsonValue &value, QStringList keys) {
	keys.sort();
	return value.isObject() && value.toObject().keys() == keys;
}

} // namespace

bool ValidExportPath(const QString &value) {
	if (value.isEmpty()) {
		return true;
	} else if (value.size() > kMaxExportPathLength) {
		return false;
	}
	for (const auto &ch : value) {
		if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
			return false;
		}
	}
	return QDir::isAbsolutePath(value);
}

QString SanitizeExportDirName(const QString &name) {
	auto result = QString();
	result.reserve(name.size());
	for (const auto &ch : name) {
		if (!ForbiddenInName(ch)) {
			result.append(ch);
		}
	}
	result = TrimName(std::move(result));
	if (result.size() > kMaxExportDirLength) {
		result.truncate(kMaxExportDirLength);
		if (result.back().isHighSurrogate()) {
			result.chop(1);
		}
		result = TrimName(std::move(result));
	}
	return result;
}

bool SafeExportDirName(const QString &name) {
	return !name.isEmpty()
		&& name.size() <= kMaxExportDirLength + 24
		&& !ReservedName(name)
		&& SanitizeExportDirName(name).size() == name.size()
		&& TrimName(name) == name;
}

QString ExportDirWithId(const QString &name, quint64 id) {
	auto base = name;
	if (ReservedName(base)) {
		base.replace(u'.', u'_');
	}
	return base + u'_' + QString::number(id);
}

std::vector<QString> ExportDirNames(
		const std::vector<ExportSetInfo> &sets,
		int naming) {
	auto result = std::vector<QString>();
	result.reserve(sets.size());
	for (const auto &set : sets) {
		const auto name = BaseName(set, naming);
		result.push_back((name.isEmpty() || ReservedName(name))
			? ExportDirWithId(name, set.id)
			: name);
	}
	for (auto round = 0; round != 4; ++round) {
		auto counts = std::map<QString, int>();
		for (const auto &name : result) {
			++counts[name.toCaseFolded()];
		}
		auto changed = false;
		for (auto i = 0; i != int(result.size()); ++i) {
			if (counts[result[i].toCaseFolded()] > 1) {
				result[i] = ExportDirWithId(result[i], sets[i].id);
				changed = true;
			}
		}
		if (!changed) {
			break;
		}
	}
	return result;
}

QString ExportFileName(quint64 id, const QString &extension) {
	return QString::number(id) + u'.' + extension;
}

bool SafeExportFileName(const QString &name) {
	const auto dot = name.indexOf(u'.');
	if (dot <= 0) {
		return false;
	}
	const auto extension = name.mid(dot + 1);
	auto ok = false;
	const auto id = name.left(dot).toULongLong(&ok);
	return ok
		&& ExportFileName(id, extension) == name
		&& (extension == u"webp"_q
			|| extension == u"tgs"_q
			|| extension == u"webm"_q);
}

std::optional<ExportManifest> ParseExportManifest(const QByteArray &bytes) {
	const auto object = ParseVersioned(bytes, { u"version"_q, u"sets"_q });
	if (!object || !object->value(u"sets"_q).isArray()) {
		return std::nullopt;
	}
	auto result = ExportManifest();
	auto ids = std::set<quint64>();
	auto dirs = std::set<QString>();
	for (const auto &value : object->value(u"sets"_q).toArray()) {
		if (!HasKeys(
				value,
				{ u"id"_q, u"dir"_q, u"hash"_q, u"count"_q })) {
			return std::nullopt;
		}
		const auto entry = value.toObject();
		const auto id = DecimalId(entry.value(u"id"_q));
		const auto hash = DecimalId(entry.value(u"hash"_q), true);
		const auto dir = entry.value(u"dir"_q);
		const auto count = entry.value(u"count"_q);
		const auto parsed = count.toInt(-1);
		if (!id
			|| !hash
			|| !dir.isString()
			|| !SafeExportDirName(dir.toString())
			|| !count.isDouble()
			|| count.toDouble() != double(parsed)
			|| parsed < 0
			|| !ids.insert(*id).second
			|| !dirs.insert(dir.toString().toCaseFolded()).second) {
			return std::nullopt;
		}
		result.sets.push_back({ *id, dir.toString(), *hash, parsed });
	}
	return result;
}

QByteArray SerializeExportManifest(const ExportManifest &manifest) {
	auto sets = QJsonArray();
	for (const auto &set : manifest.sets) {
		sets.push_back(QJsonObject{
			{ u"id"_q, QString::number(set.id) },
			{ u"dir"_q, set.dir },
			{ u"hash"_q, QString::number(set.hash) },
			{ u"count"_q, set.count },
		});
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, kManifestVersion },
		{ u"sets"_q, sets },
	}).toJson(QJsonDocument::Indented);
}

std::optional<ExportSetManifest> ParseExportSetManifest(
		const QByteArray &bytes) {
	const auto object = ParseVersioned(bytes, {
		u"version"_q,
		u"id"_q,
		u"shortName"_q,
		u"title"_q,
		u"stickers"_q,
	});
	if (!object) {
		return std::nullopt;
	}
	const auto id = DecimalId(object->value(u"id"_q));
	const auto shortName = object->value(u"shortName"_q);
	const auto title = object->value(u"title"_q);
	const auto stickers = object->value(u"stickers"_q);
	if (!id
		|| !shortName.isString()
		|| !title.isString()
		|| !stickers.isArray()) {
		return std::nullopt;
	}
	auto result = ExportSetManifest{
		.id = *id,
		.shortName = shortName.toString(),
		.title = title.toString(),
	};
	auto ids = std::set<quint64>();
	for (const auto &value : stickers.toArray()) {
		if (!HasKeys(value, { u"id"_q, u"file"_q, u"emoji"_q })) {
			return std::nullopt;
		}
		const auto entry = value.toObject();
		const auto sticker = DecimalId(entry.value(u"id"_q));
		const auto file = entry.value(u"file"_q);
		const auto emoji = entry.value(u"emoji"_q);
		if (!sticker
			|| !file.isString()
			|| !emoji.isString()
			|| !SafeExportFileName(file.toString())
			|| !file.toString().startsWith(QString::number(*sticker) + u'.')
			|| !ids.insert(*sticker).second) {
			return std::nullopt;
		}
		result.stickers.push_back({
			*sticker,
			file.toString(),
			emoji.toString(),
		});
	}
	return result;
}

QByteArray SerializeExportSetManifest(const ExportSetManifest &manifest) {
	auto stickers = QJsonArray();
	for (const auto &sticker : manifest.stickers) {
		stickers.push_back(QJsonObject{
			{ u"id"_q, QString::number(sticker.id) },
			{ u"file"_q, sticker.file },
			{ u"emoji"_q, sticker.emoji },
		});
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, kManifestVersion },
		{ u"id"_q, QString::number(manifest.id) },
		{ u"shortName"_q, manifest.shortName },
		{ u"title"_q, manifest.title },
		{ u"stickers"_q, stickers },
	}).toJson(QJsonDocument::Indented);
}

ExportPlan PlanExport(
		const std::optional<ExportManifest> &previous,
		const std::vector<ExportSetInfo> &current,
		int naming) {
	auto result = ExportPlan();
	const auto names = ExportDirNames(
		current,
		ValidExportDirNaming(naming) ? naming : 0);
	auto known = std::set<quint64>();
	for (auto i = 0; i != int(current.size()); ++i) {
		const auto &set = current[i];
		known.insert(set.id);
		auto entry = ExportPlanSet{ set.id, names[i], QString(), true };
		if (previous) {
			for (const auto &old : previous->sets) {
				if (old.id == set.id) {
					entry.write = (old.hash != set.hash)
						|| (old.count != set.count);
					entry.renameFrom = (old.dir != entry.dir)
						? old.dir
						: QString();
					break;
				}
			}
		}
		if (entry.write || !entry.renameFrom.isEmpty()) {
			result.sets.push_back(std::move(entry));
		}
	}
	if (previous) {
		for (const auto &old : previous->sets) {
			if (!known.contains(old.id)) {
				result.removed.push_back(old.id);
			}
		}
	}
	return result;
}

QStringList StaleExportFiles(
		const std::optional<ExportSetManifest> &previous,
		const ExportSetManifest &next) {
	auto result = QStringList();
	if (!previous || previous->id != next.id) {
		return result;
	}
	auto kept = std::set<QString>();
	for (const auto &sticker : next.stickers) {
		kept.insert(sticker.file);
	}
	for (const auto &sticker : previous->stickers) {
		if (!kept.contains(sticker.file)) {
			result.push_back(sticker.file);
		}
	}
	return result;
}

void UpsertExportSet(ExportManifest &manifest, ExportManifestSet set) {
	for (auto &entry : manifest.sets) {
		if (entry.id == set.id) {
			entry = std::move(set);
			return;
		}
	}
	manifest.sets.push_back(std::move(set));
}

void RemoveExportSets(
		ExportManifest &manifest,
		const std::vector<quint64> &ids) {
	std::erase_if(manifest.sets, [&](const ExportManifestSet &set) {
		return std::find(ids.begin(), ids.end(), set.id) != ids.end();
	});
}

int ExportRun::begin(bool manual) {
	++_generation;
	_manual = manual;
	_pending.clear();
	_status = ExportStatus{ .running = true };
	return _generation;
}

bool ExportRun::plan(int generation, const std::vector<quint64> &sets) {
	if (!accepts(generation) || _status.total) {
		return false;
	}
	_pending = std::set<quint64>(sets.begin(), sets.end());
	_status.total = int(_pending.size());
	checkFinished();
	return true;
}

void ExportRun::invalidate() {
	++_generation;
	_pending.clear();
	_status = ExportStatus();
}

bool ExportRun::accepts(int generation) const {
	return _status.running && generation == _generation;
}

bool ExportRun::setDone(int generation, quint64 id) {
	return resolve(generation, id, false);
}

bool ExportRun::setFailed(int generation, quint64 id) {
	return resolve(generation, id, true);
}

bool ExportRun::resolve(int generation, quint64 id, bool failed) {
	if (!accepts(generation) || !_pending.erase(id)) {
		return false;
	}
	++_status.done;
	_status.failed += failed ? 1 : 0;
	checkFinished();
	return true;
}

bool ExportRun::fail(int generation, ExportError error) {
	if (!accepts(generation) || error == ExportError::None) {
		return false;
	}
	_pending.clear();
	_status.running = false;
	_status.finished = true;
	_status.error = error;
	_paused = true;
	return true;
}

bool ExportRun::autoSyncPaused() const {
	return _paused;
}

const ExportStatus &ExportRun::status() const {
	return _status;
}

void ExportRun::checkFinished() {
	if (!_pending.empty()) {
		return;
	}
	_status.running = false;
	_status.finished = true;
	if (_manual) {
		_paused = false;
	}
}

} // namespace Nagram::Media
