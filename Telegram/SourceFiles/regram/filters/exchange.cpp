#include "regram/filters/exchange.h"

#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtCore/QVariant>

#include <algorithm>
#include <charconv>

namespace Regram::Filters {
namespace {

QString NewId() {
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool Space(char ch) {
	return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

bool InvalidDocument(const QByteArray &data) {
	struct Frame {
		bool object = false;
		QSet<QString> keys;
	};
	auto stack = std::vector<Frame>();
	for (auto i = qsizetype(); i < data.size(); ++i) {
		const auto ch = data[i];
		if (ch == '{' || ch == '[') {
			stack.push_back({ .object = ch == '{' });
		} else if (ch == '}' || ch == ']') {
			if (!stack.empty()) {
				stack.pop_back();
			}
		} else if (ch == '-' || (ch >= '0' && ch <= '9')) {
			auto end = i + 1;
			while (end < data.size() && !Space(data[end])
				&& data[end] != ',' && data[end] != ']' && data[end] != '}') {
				++end;
			}
			auto number = int64_t();
			const auto parsed = std::from_chars(
				data.constData() + i,
				data.constData() + end,
				number);
			if (parsed.ec != std::errc() || parsed.ptr != data.constData() + end) {
				return true;
			}
			i = end - 1;
		} else if (ch == '"') {
			const auto start = i++;
			while (i < data.size() && data[i] != '"') {
				i += (data[i] == '\\') ? 2 : 1;
			}
			auto next = i + 1;
			while (next < data.size() && Space(data[next])) {
				++next;
			}
			if (!stack.empty() && stack.back().object
				&& next < data.size() && data[next] == ':') {
				const auto key = QJsonDocument::fromJson(
					"[" + data.mid(start, i - start + 1) + "]"
				).array().first().toString();
				if (stack.back().keys.contains(key)) {
					return true;
				}
				stack.back().keys.insert(key);
			}
		}
	}
	return false;
}

QString PeerKindName(PeerKind kind) {
	switch (kind) {
	case PeerKind::User: return u"user"_q;
	case PeerKind::Group: return u"group"_q;
	case PeerKind::Channel: return u"channel"_q;
	}
	return {};
}

std::optional<PeerKind> ParsePeerKind(const QString &kind) {
	if (kind == u"user"_q) {
		return PeerKind::User;
	} else if (kind == u"group"_q) {
		return PeerKind::Group;
	} else if (kind == u"channel"_q) {
		return PeerKind::Channel;
	}
	return std::nullopt;
}

bool ReadLegacyScopes(const QJsonArray &scopes, Rule &rule) {
	if (scopes.size() > kMaxScopes) {
		return false;
	}
	for (const auto &scope : scopes) {
		const auto value = scope.toVariant();
		if (value.metaType().id() != QMetaType::LongLong) {
			return false;
		}
		const auto packed = value.toLongLong();
		if (const auto peer = FromIosPeerId(packed)) {
			rule.scopes.push_back(*peer);
		} else {
			rule.unresolvedScopes.push_back(
				(u"ios:"_q + QString::number(packed)).toStdString());
		}
	}
	return true;
}

bool ReadScopes(const QJsonArray &scopes, Rule &rule) {
	if (scopes.size() > kMaxScopes) {
		return false;
	}
	for (const auto &scope : scopes) {
		if (!scope.isObject()) {
			return false;
		}
		const auto object = scope.toObject();
		const auto raw = QJsonDocument(object).toJson(QJsonDocument::Compact);
		if (raw.size() > 256) {
			return false;
		}
		const auto kind = ParsePeerKind(object.value(u"kind"_q).toString());
		const auto idValue = object.value(u"id"_q);
		const auto threadValue = object.value(u"thread"_q);
		const auto id = idValue.isString()
			? ParseServerId(idValue.toString().toStdString())
			: std::nullopt;
		const auto thread = threadValue.isString()
			? ParseServerId(threadValue.toString().toStdString(), true)
			: std::nullopt;
		if (!kind || !id || !thread || object.size() != 3) {
			rule.unresolvedScopes.push_back(raw.toStdString());
		} else {
			rule.scopes.push_back({ *kind, *id, *thread });
		}
	}
	return true;
}

bool ReadRule(const QJsonValue &value, int version, Rule &rule) {
	if (!value.isObject()) {
		return false;
	}
	const auto object = value.toObject();
	const auto legacy = (version == 0);
	const auto id = object.value(u"id"_q);
	const auto pattern = object.value(u"pattern"_q);
	const auto regex = object.value(legacy ? u"isRegex"_q : u"regex"_q);
	const auto scopes = object.value(legacy ? u"peerIds"_q : u"scopes"_q);
	if (!id.isString() || id.toString().isEmpty() || id.toString().size() > 128
		|| !pattern.isString() || pattern.toString().size() > kMaxPatternUnits
		|| !regex.isBool() || !scopes.isArray()) {
		return false;
	}
	if (QString::fromUtf8(pattern.toString().toUtf8()) != pattern.toString()
		|| QString::fromUtf8(id.toString().toUtf8()) != id.toString()) {
		return false;
	}
	rule.id = id.toString().toStdString();
	rule.pattern = pattern.toString().toStdU16String();
	rule.regex = regex.toBool();
	if (legacy) {
		const auto enabled = object.value(u"isEnabled"_q);
		const auto exception = object.value(u"isException"_q);
		if ((!enabled.isUndefined() && !enabled.isNull() && !enabled.isBool())
			|| (!exception.isUndefined() && !exception.isNull() && !exception.isBool())
			|| object.size() != 4 + object.contains(u"isEnabled"_q)
				+ object.contains(u"isException"_q)) {
			return false;
		}
		rule.enabled = enabled.toBool(true);
		rule.exception = exception.toBool(false);
		return ReadLegacyScopes(scopes.toArray(), rule);
	}
	const auto enabled = object.value(u"enabled"_q);
	const auto exception = object.value(u"exception"_q);
	const auto unresolved = object.value(u"unresolvedScopes"_q);
	if (!enabled.isBool() || !unresolved.isArray()
		|| object.size() != (version == 1 ? 6 : 7)
		|| (version == 2 && !exception.isBool())
		|| unresolved.toArray().size() > kMaxScopes) {
		return false;
	}
	rule.enabled = enabled.toBool();
	rule.exception = exception.toBool(false);
	for (const auto &entry : unresolved.toArray()) {
		if (!entry.isString() || entry.toString().size() > 256) {
			return false;
		}
		rule.unresolvedScopes.push_back(entry.toString().toStdString());
	}
	return ReadScopes(scopes.toArray(), rule);
}

} // namespace

ImportResult ImportRules(const QByteArray &data) {
	if (data.size() > kMaxImportBytes) {
		return { .error = u"Rule file exceeds the size limit"_q };
	}
	auto parse = QJsonParseError();
	const auto document = QJsonDocument::fromJson(data, &parse);
	if (parse.error != QJsonParseError::NoError || InvalidDocument(data)
		|| QString::fromUtf8(data).toUtf8() != data) {
		return { .error = u"Invalid JSON, duplicate fields or non-integer numbers"_q };
	}
	const auto legacy = document.isArray();
	const auto object = document.object();
	const auto version = legacy ? 0 : object.value(u"version"_q).toInt();
	if (!legacy && (!document.isObject() || object.size() != 3
		|| object.value(u"format"_q) != u"regram.filters"_q
		|| (version != 1 && version != 2)
		|| !object.value(u"rules"_q).isArray())) {
		return { .error = u"Unsupported rule file format or version"_q };
	}
	const auto entries = legacy ? document.array() : object.value(u"rules"_q).toArray();
	if (entries.size() > kMaxRules) {
		return { .error = u"Rule file contains too many rules"_q };
	}
	auto result = ImportResult();
	auto ids = QSet<QString>();
	auto scopeCount = size_t();
	for (const auto &entry : entries) {
		auto rule = Rule();
		if (!ReadRule(entry, version, rule)
			|| ids.contains(QString::fromStdString(rule.id))) {
			return { .error = u"Invalid rule fields, duplicate IDs or inexact legacy peer IDs"_q };
		}
		ids.insert(QString::fromStdString(rule.id));
		scopeCount += rule.scopes.size() + rule.unresolvedScopes.size();
		if (scopeCount > kMaxScopes) {
			return { .error = u"Rule file contains too many conversation scopes"_q };
		}
		if (!rule.unresolvedScopes.empty()) {
			rule.enabled = false;
		}
		result.rules.push_back(std::move(rule));
	}
	auto engine = Engine();
	if (!engine.setRules(result.rules)) {
		return { .error = u"Rule file contains too many rules"_q };
	}
	result.problems = engine.problems();
	for (auto &rule : result.rules) {
		const auto invalid = std::any_of(
			result.problems.begin(),
			result.problems.end(),
			[&](const auto &problem) {
				return problem.ruleId == rule.id;
			});
		if (invalid) {
			rule.enabled = false;
		}
	}
	return result;
}

QByteArray ExportRules(const std::vector<Rule> &rules) {
	auto entries = QJsonArray();
	for (const auto &rule : rules) {
		auto scopes = QJsonArray();
		for (const auto &peer : rule.scopes) {
			scopes.append(QJsonObject{
				{ u"kind"_q, PeerKindName(peer.kind) },
				{ u"id"_q, QString::number(qulonglong(peer.id)) },
				{ u"thread"_q, QString::number(qulonglong(peer.thread)) },
			});
		}
		auto unresolved = QJsonArray();
		for (const auto &scope : rule.unresolvedScopes) {
			unresolved.append(QString::fromStdString(scope));
		}
		entries.append(QJsonObject{
			{ u"id"_q, QString::fromStdString(rule.id) },
			{ u"pattern"_q, QString::fromStdU16String(rule.pattern) },
			{ u"regex"_q, rule.regex },
			{ u"enabled"_q, rule.enabled },
			{ u"exception"_q, rule.exception },
			{ u"scopes"_q, scopes },
			{ u"unresolvedScopes"_q, unresolved },
		});
	}
	return QJsonDocument(QJsonObject{
		{ u"format"_q, u"regram.filters"_q },
		{ u"version"_q, 2 },
		{ u"rules"_q, entries },
	}).toJson(QJsonDocument::Indented);
}

ExportResult ExportIosRules(const std::vector<Rule> &rules) {
	auto entries = QJsonArray();
	for (const auto &rule : rules) {
		if (!rule.unresolvedScopes.empty()) {
			return { .error = u"Unresolved scopes require a Desktop backup"_q };
		}
		auto scopes = QJsonArray();
		for (const auto &peer : rule.scopes) {
			const auto packed = ToIosPeerId(peer);
			if (!packed) {
				return { .error = u"Topic scopes or unsupported peer IDs require a Desktop backup"_q };
			}
			scopes.append(qint64(*packed));
		}
		entries.append(QJsonObject{
			{ u"id"_q, QString::fromStdString(rule.id) },
			{ u"pattern"_q, QString::fromStdU16String(rule.pattern) },
			{ u"isRegex"_q, rule.regex },
			{ u"isEnabled"_q, rule.enabled },
			{ u"isException"_q, rule.exception },
			{ u"peerIds"_q, scopes },
		});
	}
	const auto data = QJsonDocument(entries).toJson(QJsonDocument::Indented);
	const auto checked = ImportRules(data);
	if (!checked.error.isEmpty()) {
		return { .error = checked.error };
	}
	return { .data = data };
}

bool IsRulesFileName(const QString &name) {
	const auto lower = name.toLower();
	return lower.endsWith(u".json"_q)
		&& (lower.startsWith(u"regram-message-filter"_q)
			|| lower.startsWith(u"regram-filters"_q));
}

MergeResult MergeRules(
		const std::vector<Rule> &existing,
		const std::vector<Rule> &imported) {
	auto result = MergeResult{ .rules = existing };
	auto scopeCount = size_t();
	for (const auto &rule : existing) {
		scopeCount += rule.scopes.size() + rule.unresolvedScopes.size();
	}
	for (const auto &rule : imported) {
		const auto duplicate = std::any_of(
			result.rules.begin(),
			result.rules.end(),
			[&](const auto &entry) {
				return SameRule(rule, entry);
			});
		if (duplicate) {
			++result.duplicates;
			continue;
		} else if (result.rules.size() >= kMaxRules
			|| scopeCount + rule.scopes.size() + rule.unresolvedScopes.size()
				> kMaxScopes) {
			return {
				.rules = existing,
				.error = u"Merged rule set exceeds the size limit"_q,
			};
		}
		auto copy = rule;
		copy.id = NewId().toStdString();
		result.rules.push_back(std::move(copy));
		scopeCount += rule.scopes.size() + rule.unresolvedScopes.size();
		++result.added;
	}
	return result;
}

} // namespace Regram::Filters
