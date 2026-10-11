#include "regram/nagram/notifications/model.h"

#include "regram/nagram/core/regex.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

#include <algorithm>
#include <map>

namespace Nagram::Notifications {
namespace {

constexpr auto kMaxText = 16384;
constexpr auto kMaxCachedPatterns = 128;

[[nodiscard]] std::optional<QJsonObject> Document(
		const QByteArray &raw,
		QStringList keys) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	const auto object = document.object();
	keys.push_back(u"version"_q);
	keys.sort();
	const auto version = object.value(u"version"_q);
	return (object.keys() == keys
		&& version.isDouble()
		&& version.toDouble() == 1.)
		? std::make_optional(object)
		: std::nullopt;
}

[[nodiscard]] bool Flags(const QJsonObject &object, QStringList keys) {
	return std::ranges::all_of(keys, [&](const QString &key) {
		return object.value(key).isBool();
	});
}

[[nodiscard]] std::optional<int> Integer(
		const QJsonValue &value,
		int from,
		int till) {
	const auto parsed = value.toInt(from - 1);
	return (value.isDouble()
		&& value.toDouble() == double(parsed)
		&& parsed >= from
		&& parsed < till)
		? std::make_optional(parsed)
		: std::nullopt;
}

[[nodiscard]] bool Selected(const QuietHours &config, int weekday) {
	return config.weekdays.empty()
		|| (std::ranges::find(config.weekdays, weekday)
			!= config.weekdays.end());
}

[[nodiscard]] QRegularExpression::PatternOptions Options(
		const KeywordRule &rule) {
	return rule.caseSensitive
		? QRegularExpression::NoPatternOption
		: QRegularExpression::CaseInsensitiveOption;
}

[[nodiscard]] bool ValidRule(const KeywordRule &rule) {
	return !rule.pattern.trimmed().isEmpty()
		&& rule.pattern.size() <= kMaxKeywordLength
		&& !rule.pattern.contains(u'\n')
		&& !rule.pattern.contains(u'\r')
		&& !rule.pattern.contains(QChar(0))
		&& (!rule.regex || !Regex::Check(rule.pattern, Options(rule)));
}

[[nodiscard]] bool Unique(const std::vector<KeywordRule> &rules) {
	for (auto i = rules.begin(); i != rules.end(); ++i) {
		if (std::find(i + 1, rules.end(), *i) != rules.end()) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] const QRegularExpression &Compiled(const KeywordRule &rule) {
	static auto cache = std::map<
		std::pair<QString, bool>,
		QRegularExpression>();
	const auto key = std::pair(rule.pattern, rule.caseSensitive);
	if (const auto i = cache.find(key); i != cache.end()) {
		return i->second;
	} else if (cache.size() >= kMaxCachedPatterns) {
		cache.clear();
	}
	auto expression = Regex::Compile(rule.pattern, Options(rule));
	expression.optimize();
	return cache.emplace(key, std::move(expression)).first->second;
}

[[nodiscard]] std::optional<KeywordRule> ParseExpression(
		const QString &line) {
	const auto insensitive = line.endsWith(u"/i"_q);
	const auto tail = insensitive ? 2 : 1;
	if (!line.startsWith(u'/')
		|| (!insensitive && !line.endsWith(u'/'))
		|| line.size() <= 1 + tail) {
		return std::nullopt;
	}
	return KeywordRule{
		.pattern = line.mid(1, line.size() - 1 - tail),
		.regex = true,
		.caseSensitive = !insensitive,
	};
}

} // namespace

bool ValidPolicy(const QString &value) {
	return value == u"default"_q || value == u"silenced"_q || value == u"disabled"_q;
}

bool ValidPolicyExceptions(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	const auto document = QJsonDocument::fromJson(raw);
	if (!document.isObject() || document.object().size() > 10000) {
		return false;
	}
	const auto values = document.object();
	for (auto i = values.begin(); i != values.end(); ++i) {
		auto ok = false;
		const auto id = i.key().toULongLong(&ok);
		if (!ok || !id || QString::number(id) != i.key()
			|| !i.value().isString() || !ValidPolicy(i.value().toString())) {
			return false;
		}
	}
	return true;
}

QString PolicyForPeer(
		const QString &fallback,
		const QByteArray &exceptions,
		const QString &peer) {
	return PolicyForPeer(fallback, QJsonDocument::fromJson(exceptions).object(), peer);
}

QString PolicyForPeer(
		const QString &fallback,
		const QJsonObject &exceptions,
		const QString &peer) {
	const auto value = exceptions.value(peer).toString();
	return ValidPolicy(value) ? value : fallback;
}

std::optional<QuietHours> ParseQuietHours(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return QuietHours();
	}
	const auto flags = QStringList{
		u"enabled"_q,
		u"allowContacts"_q,
		u"allowPinned"_q,
		u"allowMentions"_q,
		u"allowKeywords"_q,
	};
	const auto object = Document(
		raw,
		flags + QStringList{ u"start"_q, u"end"_q, u"weekdays"_q });
	if (!object || !Flags(*object, flags)) {
		return std::nullopt;
	}
	const auto start = Integer(object->value(u"start"_q), 0, kMinutesPerDay);
	const auto end = Integer(object->value(u"end"_q), 0, kMinutesPerDay);
	const auto days = object->value(u"weekdays"_q);
	if (!start || !end || !days.isArray() || days.toArray().size() > 7) {
		return std::nullopt;
	}
	auto weekdays = std::vector<int>();
	for (const auto &entry : days.toArray()) {
		const auto day = Integer(entry, 1, 8);
		if (!day || (!weekdays.empty() && *day <= weekdays.back())) {
			return std::nullopt;
		}
		weekdays.push_back(*day);
	}
	return QuietHours{
		.enabled = object->value(u"enabled"_q).toBool(),
		.start = *start,
		.end = *end,
		.weekdays = std::move(weekdays),
		.allowContacts = object->value(u"allowContacts"_q).toBool(),
		.allowPinned = object->value(u"allowPinned"_q).toBool(),
		.allowMentions = object->value(u"allowMentions"_q).toBool(),
		.allowKeywords = object->value(u"allowKeywords"_q).toBool(),
	};
}

QByteArray Serialize(const QuietHours &value) {
	if (value == QuietHours()) {
		return QByteArray();
	}
	auto weekdays = value.weekdays;
	std::ranges::sort(weekdays);
	weekdays.erase(std::ranges::unique(weekdays).begin(), weekdays.end());
	auto days = QJsonArray();
	for (const auto day : weekdays) {
		days.push_back(day);
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"enabled"_q, value.enabled },
		{ u"start"_q, value.start },
		{ u"end"_q, value.end },
		{ u"weekdays"_q, days },
		{ u"allowContacts"_q, value.allowContacts },
		{ u"allowPinned"_q, value.allowPinned },
		{ u"allowMentions"_q, value.allowMentions },
		{ u"allowKeywords"_q, value.allowKeywords },
	}).toJson(QJsonDocument::Compact);
}

bool ValidQuietHours(const QByteArray &raw) {
	return ParseQuietHours(raw).has_value();
}

std::optional<int> ParseMinute(const QString &text) {
	const auto parts = text.trimmed().split(u':');
	if (parts.size() != 2 || parts[0].size() > 2 || parts[1].size() != 2) {
		return std::nullopt;
	}
	auto hoursOk = false;
	auto minutesOk = false;
	const auto hours = parts[0].toUInt(&hoursOk);
	const auto minutes = parts[1].toUInt(&minutesOk);
	return (hoursOk && minutesOk && hours < 24 && minutes < 60)
		? std::make_optional(int(hours * 60 + minutes))
		: std::nullopt;
}

QString FormatMinute(int minute) {
	return u"%1:%2"_q
		.arg(minute / 60, 2, 10, QChar(u'0'))
		.arg(minute % 60, 2, 10, QChar(u'0'));
}

bool Active(const QuietHours &config, int weekday, int minute) {
	if (!config.enabled) {
		return false;
	} else if (config.start == config.end) {
		return Selected(config, weekday);
	} else if (config.start < config.end) {
		return Selected(config, weekday)
			&& (minute >= config.start)
			&& (minute < config.end);
	}
	// A period that crosses midnight belongs to the day it starts on.
	const auto previous = (weekday == 1) ? 7 : (weekday - 1);
	return (minute >= config.start && Selected(config, weekday))
		|| (minute < config.end && Selected(config, previous));
}

bool Silences(
		const QuietHours &config,
		const Facts &facts,
		int weekday,
		int minute) {
	if (!Active(config, weekday, minute)) {
		return false;
	} else if (!facts.message) {
		return true;
	}
	return !((config.allowContacts && facts.contact)
		|| (config.allowPinned && facts.pinned)
		|| (config.allowMentions && facts.mention)
		|| (config.allowKeywords && facts.keyword));
}

std::optional<KeywordAlerts> ParseKeywordAlerts(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return KeywordAlerts();
	}
	const auto flags = QStringList{ u"enabled"_q, u"channels"_q };
	const auto object = Document(raw, flags + QStringList{ u"rules"_q });
	const auto rules = object ? object->value(u"rules"_q) : QJsonValue();
	if (!object
		|| !Flags(*object, flags)
		|| !rules.isArray()
		|| rules.toArray().size() > kMaxKeywordRules) {
		return std::nullopt;
	}
	auto result = KeywordAlerts{
		.enabled = object->value(u"enabled"_q).toBool(),
		.channels = object->value(u"channels"_q).toBool(),
	};
	const auto ruleFlags = QStringList{ u"regex"_q, u"caseSensitive"_q };
	for (const auto &entry : rules.toArray()) {
		const auto rule = entry.toObject();
		if (!entry.isObject()
			|| rule.size() != 3
			|| !rule.value(u"pattern"_q).isString()
			|| !Flags(rule, ruleFlags)) {
			return std::nullopt;
		}
		result.rules.push_back({
			.pattern = rule.value(u"pattern"_q).toString(),
			.regex = rule.value(u"regex"_q).toBool(),
			.caseSensitive = rule.value(u"caseSensitive"_q).toBool(),
		});
		if (!ValidRule(result.rules.back())) {
			return std::nullopt;
		}
	}
	return Unique(result.rules) ? std::make_optional(result) : std::nullopt;
}

QByteArray Serialize(const KeywordAlerts &value) {
	if (value == KeywordAlerts()) {
		return QByteArray();
	}
	auto rules = QJsonArray();
	for (const auto &rule : value.rules) {
		rules.push_back(QJsonObject{
			{ u"pattern"_q, rule.pattern },
			{ u"regex"_q, rule.regex },
			{ u"caseSensitive"_q, rule.caseSensitive },
		});
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"enabled"_q, value.enabled },
		{ u"channels"_q, value.channels },
		{ u"rules"_q, rules },
	}).toJson(QJsonDocument::Compact);
}

bool ValidKeywordAlerts(const QByteArray &raw) {
	return ParseKeywordAlerts(raw).has_value();
}

std::vector<KeywordRule> ParseKeywordLines(const QString &text) {
	auto result = std::vector<KeywordRule>();
	for (const auto &raw : text.split(u'\n')) {
		const auto line = raw.trimmed();
		if (line.isEmpty()) {
			continue;
		}
		auto rule = ParseExpression(line).value_or(KeywordRule{
			.pattern = line,
		});
		if (std::ranges::find(result, rule) == result.end()) {
			result.push_back(std::move(rule));
		}
	}
	return result;
}

QString FormatKeywordLines(const std::vector<KeywordRule> &rules) {
	auto lines = QStringList();
	for (const auto &rule : rules) {
		lines.push_back(!rule.regex
			? rule.pattern
			: (u'/' + rule.pattern + (rule.caseSensitive ? u"/"_q : u"/i"_q)));
	}
	return lines.join(u'\n');
}

std::optional<KeywordProblem> CheckKeywordRules(
		const std::vector<KeywordRule> &rules) {
	if (rules.size() > kMaxKeywordRules) {
		return KeywordProblem{ .line = kMaxKeywordRules + 1 };
	}
	for (auto i = 0; i != int(rules.size()); ++i) {
		const auto &rule = rules[i];
		if (ValidRule(rule)) {
			continue;
		}
		const auto problem = rule.regex
			? Regex::Check(rule.pattern, Options(rule))
			: std::nullopt;
		return KeywordProblem{ i + 1, problem ? problem->text : QString() };
	}
	return std::nullopt;
}

bool Matches(const KeywordAlerts &config, const QString &text, bool channel) {
	if (!config.enabled || text.isEmpty() || (channel && !config.channels)) {
		return false;
	}
	const auto checked = text.left(kMaxText);
	return std::ranges::any_of(config.rules, [&](const KeywordRule &rule) {
		return rule.regex
			? (Regex::Find(Compiled(rule), checked) == Regex::Found::Yes)
			: checked.contains(rule.pattern, rule.caseSensitive
				? Qt::CaseSensitive
				: Qt::CaseInsensitive);
	});
}

} // namespace Nagram::Notifications
