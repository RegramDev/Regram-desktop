#include "regram/nagram/filters/model.h"

#include "regram/nagram/core/regex.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QCache>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QUuid>

#include <algorithm>
#include <memory>
#include <optional>

namespace Nagram::Filters {
namespace {

constexpr auto kMaxScopes = 200;
constexpr auto kMaxDisabledRules = 64;
constexpr auto kMaxText = 16384;
constexpr auto kMaxMatches = 256;
constexpr auto kMaxWorkMs = 20;
constexpr auto kMaxConfigBytes = 128 * 1024;

QRegularExpression Compile(const QJsonObject &rule) {
	return Regex::Compile(
		rule.value(u"pattern"_q).toString(),
		rule.value(u"caseInsensitive"_q).toBool()
			? QRegularExpression::CaseInsensitiveOption
			: QRegularExpression::NoPatternOption);
}

struct CompiledRule {
	QRegularExpression expression;
	QString action;
	QString replacement;
	bool reversed = false;
};

struct CompiledConfig {
	QJsonObject config;
	std::vector<CompiledRule> rules;
	QString error;
};

const CompiledConfig &Prepared(const QByteArray &raw) {
	static auto cache = QCache<QByteArray, CompiledConfig>(32);
	static const auto oversized = CompiledConfig{
		.error = u"invalid filter configuration"_q,
	};
	if (raw.size() > kMaxConfigBytes) {
		return oversized;
	} else if (const auto found = cache.object(raw)) {
		return *found;
	}
	auto result = std::make_unique<CompiledConfig>();
	if (!Validate(raw)) {
		result->error = oversized.error;
	} else {
		result->config = QJsonDocument::fromJson(raw).object();
		for (const auto &entry : result->config.value(u"rules"_q).toArray()) {
			const auto rule = entry.toObject();
			if (rule.value(u"enabled"_q).toBool()) {
				result->rules.push_back({
					.expression = Compile(rule),
					.action = rule.value(u"action"_q).toString(),
					.replacement = rule.value(u"replacement"_q).toString(),
					.reversed = rule.value(u"reversed"_q).toBool(),
				});
			}
		}
	}
	const auto pointer = result.get();
	cache.insert(raw, result.release());
	return *pointer;
}

bool IdList(const QJsonValue &value) {
	if (!value.isArray() || value.toArray().size() > 1000) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &entry : value.toArray()) {
		const auto id = entry.toString();
		auto ok = false;
		const auto number = id.toULongLong(&ok);
		if (!entry.isString() || !ok || !number
			|| QString::number(number) != id || seen.contains(id)) {
			return false;
		}
		seen.insert(id);
	}
	return true;
}

bool ValidText(const QJsonValue &value, int limit, bool allowEmpty) {
	if (!value.isString()) {
		return false;
	}
	const auto text = value.toString();
	return (allowEmpty || !text.isEmpty()) && text.size() <= limit
		&& !text.contains(QChar(0))
		&& QString::fromUtf8(text.toUtf8()) == text;
}

bool CanonicalUuid(const QString &id) {
	const auto uuid = QUuid(id);
	return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id;
}

bool ValidRule(const QJsonObject &rule) {
	const auto action = rule.value(u"action"_q).toString();
	return rule.size() == 8
		&& CanonicalUuid(rule.value(u"id"_q).toString())
		&& ValidText(rule.value(u"title"_q), 128, false)
		&& ValidText(rule.value(u"pattern"_q), 2048, false)
		&& ValidText(rule.value(u"replacement"_q), 4096, true)
		&& rule.value(u"enabled"_q).isBool()
		&& rule.value(u"caseInsensitive"_q).isBool()
		&& rule.value(u"reversed"_q).isBool()
		&& (action == u"mask"_q || action == u"replace"_q
			|| action == u"hide"_q)
		&& (!rule.value(u"reversed"_q).toBool() || action == u"hide"_q)
		&& Compile(rule).isValid();
}

bool ValidRules(const QJsonValue &value, QSet<QString> &seen) {
	if (!value.isArray() || value.toArray().size() > kMaxRules) {
		return false;
	}
	for (const auto &entry : value.toArray()) {
		const auto rule = entry.toObject();
		const auto id = rule.value(u"id"_q).toString();
		if (!entry.isObject() || seen.contains(id) || !ValidRule(rule)) {
			return false;
		}
		seen.insert(id);
	}
	return true;
}

bool ValidRuleIds(const QJsonValue &value) {
	if (!value.isArray() || value.toArray().size() > kMaxDisabledRules) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &entry : value.toArray()) {
		const auto id = entry.toString();
		if (!entry.isString() || !CanonicalUuid(id) || seen.contains(id)) {
			return false;
		}
		seen.insert(id);
	}
	return true;
}

bool DecimalId(const QJsonValue &value, bool allowZero) {
	const auto id = value.toString();
	auto ok = false;
	const auto number = id.toULongLong(&ok);
	return value.isString() && ok && (number || allowZero)
		&& QString::number(number) == id;
}

bool Versioned(const QJsonObject &config, const QJsonObject &defaults) {
	return config.keys() == defaults.keys()
		&& config.value(u"version"_q).isDouble()
		&& config.value(u"version"_q).toDouble() == 1.
		&& QJsonDocument(config).toJson(QJsonDocument::Compact).size()
			<= kMaxConfigBytes;
}

std::optional<QJsonObject> Parse(const QByteArray &raw) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	return (error.error == QJsonParseError::NoError && document.isObject())
		? std::make_optional(document.object())
		: std::nullopt;
}

bool ValidObject(const QJsonObject &config) {
	if (config.keys() != Defaults().keys()
		|| config.value(u"version"_q) != 1
		|| QJsonDocument(config).toJson(QJsonDocument::Compact).size()
			> kMaxConfigBytes) {
		return false;
	}
	for (const auto &key : { u"enabled"_q, u"filterOutgoing"_q,
			u"hideBlocked"_q, u"stripZalgo"_q }) {
		if (!config.value(key).isBool()) {
			return false;
		}
	}
	auto seen = QSet<QString>();
	return IdList(config.value(u"hiddenAuthors"_q))
		&& IdList(config.value(u"excludedPeers"_q))
		&& ValidRules(config.value(u"rules"_q), seen);
}

bool ValidScopesObject(const QJsonObject &config) {
	if (!Versioned(config, ScopeDefaults())
		|| !config.value(u"account"_q).isObject()
		|| !config.value(u"scopes"_q).isArray()
		|| config.value(u"scopes"_q).toArray().size() > kMaxScopes) {
		return false;
	}
	const auto own = config.value(u"account"_q).toObject();
	if (own.size() != 2
		|| !own.value(u"inheritGlobal"_q).isBool()
		|| !ValidRuleIds(own.value(u"disabledRules"_q))) {
		return false;
	}
	const auto keys = NewScope(QString(), QString()).keys();
	auto seen = QSet<QString>();
	auto places = QSet<QString>();
	for (const auto &entry : config.value(u"scopes"_q).toArray()) {
		const auto scope = entry.toObject();
		const auto state = scope.value(u"enabled"_q);
		const auto place = scope.value(u"peer"_q).toString()
			+ u':' + scope.value(u"topic"_q).toString();
		if (!entry.isObject() || scope.keys() != keys
			|| !DecimalId(scope.value(u"peer"_q), false)
			|| !DecimalId(scope.value(u"topic"_q), true)
			|| places.contains(place)
			|| (state != u"inherit"_q && state != u"on"_q
				&& state != u"off"_q)
			|| !ValidRuleIds(scope.value(u"disabledRules"_q))
			|| !ValidRules(scope.value(u"rules"_q), seen)) {
			return false;
		}
		places.insert(place);
	}
	return true;
}

struct Layers {
	QJsonObject config;
	QJsonObject own;
	QJsonObject chat;
	QJsonObject thread;
	QJsonArray global;
	bool overridden = false;
};

std::optional<Layers> ReadLayers(
		const QByteArray &account,
		const QByteArray &global,
		const QByteArray &scopes,
		const QString &peer,
		const QString &topic) {
	if (!Validate(account)) {
		return std::nullopt;
	}
	auto result = Layers{
		.config = account.isEmpty() ? Defaults() : *Parse(account),
		.own = ScopeDefaults().value(u"account"_q).toObject(),
	};
	if (!global.isEmpty() && ValidateGlobal(global)) {
		result.global = Parse(global)->value(u"rules"_q).toArray();
		result.overridden = true;
	}
	if (!scopes.isEmpty() && ValidateScopes(scopes)) {
		const auto settings = *Parse(scopes);
		result.own = settings.value(u"account"_q).toObject();
		result.overridden = true;
		for (const auto &entry : settings.value(u"scopes"_q).toArray()) {
			const auto scope = entry.toObject();
			const auto place = scope.value(u"topic"_q).toString();
			if (scope.value(u"peer"_q).toString() != peer) {
				continue;
			} else if (place == u"0"_q) {
				result.chat = scope;
			} else if (place == topic) {
				result.thread = scope;
			}
		}
	}
	return result;
}

void RemoveRules(QJsonArray &rules, const QJsonValue &ids) {
	const auto disabled = ids.toArray();
	for (auto i = rules.size(); i != 0; --i) {
		if (disabled.contains(rules[i - 1].toObject().value(u"id"_q))) {
			rules.removeAt(i - 1);
		}
	}
}

bool AppendRules(
		QJsonArray &rules,
		QSet<QString> &seen,
		const QJsonValue &added) {
	auto unique = true;
	for (const auto &entry : added.toArray()) {
		const auto id = entry.toObject().value(u"id"_q).toString();
		unique = unique && !seen.contains(id);
		seen.insert(id);
		rules.push_back(entry);
	}
	return unique;
}

bool ExplicitState(const QJsonObject &scope) {
	const auto state = scope.value(u"enabled"_q).toString();
	return state == u"on"_q || state == u"off"_q;
}

bool Excluded(const Layers &layers, const QString &peer) {
	return layers.chat.isEmpty()
		&& layers.config.value(u"excludedPeers"_q).toArray().contains(peer);
}

struct Edit {
	int start = 0;
	int end = 0;
	QString replacement;
};

bool ApplyEdits(TextWithEntities &text, const std::vector<Edit> &edits) {
	const auto original = text;
	auto offsets = std::vector<int>(original.text.size() + 1, -1);
	auto result = TextWithEntities();
	auto cursor = 0;
	for (const auto &edit : edits) {
		if (edit.start < cursor || edit.end <= edit.start
			|| edit.end > original.text.size()) {
			return false;
		}
		for (; cursor < edit.start; ++cursor) {
			offsets[cursor] = result.text.size();
			result.text += original.text[cursor];
		}
		offsets[edit.start] = result.text.size();
		result.text += edit.replacement;
		cursor = edit.end;
		offsets[cursor] = result.text.size();
	}
	for (; cursor < original.text.size(); ++cursor) {
		offsets[cursor] = result.text.size();
		result.text += original.text[cursor];
	}
	offsets.back() = result.text.size();
	if (result.text.size() > kMaxText) {
		return false;
	}
	for (const auto &entity : original.entities) {
		if (!entity.validForText(original.text.size())) {
			return false;
		}
		const auto start = entity.offset();
		const auto end = start + entity.length();
		const auto overlaps = std::any_of(edits.begin(), edits.end(), [&](const Edit &edit) {
			return edit.start < end && edit.end > start;
		});
		if (overlaps || offsets[start] < 0 || offsets[end] < offsets[start]) {
			continue;
		}
		auto adjusted = entity;
		adjusted.shiftLeft(start - offsets[start]);
		adjusted.shrinkFromRight(entity.length()
			- (offsets[end] - offsets[start]));
		result.entities.push_back(std::move(adjusted));
	}
	text = std::move(result);
	return true;
}

std::vector<Edit> ZalgoEdits(const QString &text) {
	auto edits = std::vector<Edit>();
	auto marks = 0;
	for (auto i = 0; i < text.size();) {
		const auto start = i;
		auto scalar = uint(text[i++].unicode());
		if (QChar::isHighSurrogate(scalar) && i < text.size()) {
			scalar = QChar::surrogateToUcs4(QChar(scalar), text[i++]);
		}
		const auto category = QChar::category(scalar);
		const auto combining = category == QChar::Mark_NonSpacing
			|| category == QChar::Mark_SpacingCombining
			|| category == QChar::Mark_Enclosing;
		marks = combining ? marks + 1 : 0;
		if (marks > 3 && scalar != 0xFE0E && scalar != 0xFE0F
			&& scalar != 0x20E3
			&& !(scalar >= 0xE0100 && scalar <= 0xE01EF)) {
			edits.push_back({ start, i, QString() });
		}
	}
	return edits;
}

} // namespace

QJsonObject Defaults() {
	return {
		{ u"version"_q, 1 },
		{ u"enabled"_q, false },
		{ u"filterOutgoing"_q, false },
		{ u"hideBlocked"_q, false },
		{ u"stripZalgo"_q, false },
		{ u"hiddenAuthors"_q, QJsonArray() },
		{ u"excludedPeers"_q, QJsonArray() },
		{ u"rules"_q, QJsonArray() },
	};
}

QJsonObject GlobalDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"rules"_q, QJsonArray() },
	};
}

QJsonObject ScopeDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"account"_q, QJsonObject{
			{ u"inheritGlobal"_q, true },
			{ u"disabledRules"_q, QJsonArray() },
		} },
		{ u"scopes"_q, QJsonArray() },
	};
}

QJsonObject NewScope(const QString &peer, const QString &topic) {
	return {
		{ u"peer"_q, peer },
		{ u"topic"_q, topic },
		{ u"enabled"_q, u"inherit"_q },
		{ u"disabledRules"_q, QJsonArray() },
		{ u"rules"_q, QJsonArray() },
	};
}

bool DefaultScope(const QJsonObject &scope) {
	return scope == NewScope(
		scope.value(u"peer"_q).toString(),
		scope.value(u"topic"_q).toString());
}

bool Validate(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	const auto config = Parse(raw);
	return config && ValidObject(*config);
}

bool ValidateGlobal(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	const auto config = Parse(raw);
	auto seen = QSet<QString>();
	return config && Versioned(*config, GlobalDefaults())
		&& ValidRules(config->value(u"rules"_q), seen);
}

bool ValidateScopes(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	const auto config = Parse(raw);
	return config && ValidScopesObject(*config);
}

Resolved Resolve(
		const QByteArray &account,
		const QByteArray &global,
		const QByteArray &scopes,
		const QString &peer,
		const QString &topic) {
	const auto layers = ReadLayers(account, global, scopes, peer, topic);
	if (!layers || !layers->overridden) {
		return { .config = account };
	}
	auto rules = QJsonArray();
	auto seen = QSet<QString>();
	auto unique = true;
	if (layers->own.value(u"inheritGlobal"_q).toBool()) {
		unique = AppendRules(rules, seen, layers->global);
	}
	RemoveRules(rules, layers->own.value(u"disabledRules"_q));
	unique = AppendRules(rules, seen, layers->config.value(u"rules"_q))
		&& unique;
	for (const auto &scope : { layers->chat, layers->thread }) {
		RemoveRules(rules, scope.value(u"disabledRules"_q));
		unique = AppendRules(rules, seen, scope.value(u"rules"_q))
			&& unique;
	}
	for (auto i = rules.size(); i != 0; --i) {
		if (!rules[i - 1].toObject().value(u"enabled"_q).toBool()) {
			rules.removeAt(i - 1);
		}
	}
	const auto count = int(rules.size());
	if (!unique) {
		return { .error = u"filter rule id conflict"_q, .count = count };
	} else if (count > kMaxRules) {
		return { .error = u"filter rule count limit"_q, .count = count };
	}
	auto config = layers->config;
	config.insert(u"enabled"_q, ExplicitState(layers->thread)
		? (layers->thread.value(u"enabled"_q) == u"on"_q)
		: ExplicitState(layers->chat)
		? (layers->chat.value(u"enabled"_q) == u"on"_q)
		: (!Excluded(*layers, peer)
			&& config.value(u"enabled"_q).toBool()));
	config.insert(u"excludedPeers"_q, QJsonArray());
	config.insert(u"rules"_q, rules);
	return {
		.config = QJsonDocument(config).toJson(QJsonDocument::Compact),
		.count = count,
	};
}

std::vector<InheritedRule> InheritedRules(
		const QByteArray &account,
		const QByteArray &global,
		const QByteArray &scopes,
		const QString &peer,
		const QString &topic) {
	auto result = std::vector<InheritedRule>();
	const auto layers = ReadLayers(account, global, scopes, peer, topic);
	if (!layers) {
		return result;
	}
	const auto add = [&](const QJsonValue &rules, Layer layer) {
		for (const auto &entry : rules.toArray()) {
			result.push_back({ entry.toObject(), layer });
		}
	};
	const auto remove = [&](const QJsonValue &ids) {
		const auto disabled = ids.toArray();
		std::erase_if(result, [&](const InheritedRule &entry) {
			return disabled.contains(entry.rule.value(u"id"_q));
		});
	};
	if (layers->own.value(u"inheritGlobal"_q).toBool()) {
		add(layers->global, Layer::Global);
	}
	remove(layers->own.value(u"disabledRules"_q));
	add(layers->config.value(u"rules"_q), Layer::Account);
	if (topic != u"0"_q) {
		remove(layers->chat.value(u"disabledRules"_q));
		add(layers->chat.value(u"rules"_q), Layer::Chat);
	}
	return result;
}

InheritedState InheritedEnabled(
		const QByteArray &account,
		const QByteArray &scopes,
		const QString &peer,
		const QString &topic) {
	const auto layers = ReadLayers(account, {}, scopes, peer, topic);
	if (!layers) {
		return {};
	} else if (topic != u"0"_q && ExplicitState(layers->chat)) {
		return {
			layers->chat.value(u"enabled"_q) == u"on"_q,
			Layer::Chat,
		};
	}
	return {
		!Excluded(*layers, peer)
			&& layers->config.value(u"enabled"_q).toBool(),
		Layer::Account,
	};
}

Result Apply(
		const QByteArray &raw,
		const TextWithEntities &source,
		const QString &author,
		const QString &peer,
		bool blocked,
		bool outgoing,
		const QString &searchable) {
	auto result = Result{ .text = source };
	if (raw.isEmpty()) {
		return result;
	}
	const auto &prepared = Prepared(raw);
	if (!prepared.error.isEmpty()) {
		result.error = prepared.error;
		return result;
	}
	const auto &config = prepared.config;
	if (!config.value(u"enabled"_q).toBool()
		|| config.value(u"excludedPeers"_q).toArray().contains(peer)
		|| (outgoing && !config.value(u"filterOutgoing"_q).toBool())) {
		return result;
	}
	if ((blocked && config.value(u"hideBlocked"_q).toBool())
		|| config.value(u"hiddenAuthors"_q).toArray().contains(author)) {
		result.hidden = true;
		return result;
	}
	if (source.text.size() > kMaxText
		|| (!searchable.isNull() && searchable.size() > kMaxText)) {
		result.error = u"filter text length limit"_q;
		return result;
	}
	auto timer = QElapsedTimer();
	timer.start();
	if (config.value(u"stripZalgo"_q).toBool()
		&& !ApplyEdits(result.text, ZalgoEdits(result.text.text))) {
		result.error = u"filter Zalgo edit failed"_q;
		return { .text = source, .error = result.error };
	}
	for (const auto &rule : prepared.rules) {
		const auto &expression = rule.expression;
		const auto &action = rule.action;
		const auto text = action == u"hide" && !searchable.isNull()
			? searchable : result.text.text;
		auto edits = std::vector<Edit>();
		auto matched = false;
		for (auto offset = 0; offset <= text.size();) {
			if (timer.elapsed() >= kMaxWorkMs) {
				result.error = u"filter runtime limit"_q;
				break;
			}
			const auto match = expression.match(text, offset);
			if (!match.isValid()) {
				result.error = u"filter regex work limit"_q;
				break;
			}
			if (!match.hasMatch()) {
				break;
			}
			if (++result.matches > kMaxMatches) {
				result.error = u"filter match count limit"_q;
				break;
			}
			matched = true;
			if (action == u"hide"_q) {
				break;
			}
			const auto start = int(match.capturedStart());
			const auto end = int(match.capturedEnd());
			if (end <= start) {
				offset = end + 1;
				continue;
			}
			edits.push_back({ start, end,
				action == u"mask"_q ? u"•••"_q
					: rule.replacement });
			offset = end;
		}
		if (!result.error.isEmpty()) {
			break;
		}
		if (action == u"hide"_q) {
			if (matched != rule.reversed) {
				result.hidden = true;
				return result;
			}
		} else if (!ApplyEdits(result.text, edits)) {
			result.error = u"filter replacement limit"_q;
			break;
		}
	}
	if (!result.error.isEmpty()) {
		return { .text = source, .error = result.error,
			.matches = result.matches };
	}
	return result;
}

} // namespace Nagram::Filters
