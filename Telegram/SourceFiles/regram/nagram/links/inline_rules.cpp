#include "regram/nagram/links/inline_rules.h"

#include "base/basic_types.h"
#include "regram/nagram/core/regex.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QUuid>

namespace Nagram::Links {
namespace {

constexpr auto kMaxInlineRules = 64;
constexpr auto kMaxInlinePatterns = 16;
constexpr auto kMaxInlinePattern = 512;
constexpr auto kMaxInlineWorkMs = 20;

[[nodiscard]] std::optional<QJsonObject> Parse(const QByteArray &raw) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	return (error.error == QJsonParseError::NoError && document.isObject())
		? std::make_optional(document.object())
		: std::nullopt;
}

[[nodiscard]] QStringList Patterns(const QJsonValue &value) {
	auto result = QStringList();
	for (const auto &entry : value.toArray()) {
		result.push_back(entry.toString());
	}
	return result;
}

[[nodiscard]] bool ValidRule(const QJsonObject &rule) {
	const auto id = rule.value(u"id"_q).toString();
	const auto uuid = QUuid(id);
	const auto patterns = rule.value(u"patterns"_q);
	if (rule.keys() != NewInlineRule(QString(), {}).keys()
		|| uuid.isNull()
		|| uuid.toString(QUuid::WithoutBraces) != id
		|| !rule.value(u"username"_q).isString()
		|| !ValidInlineUsername(
			rule.value(u"username"_q).toString())
		|| !rule.value(u"enabled"_q).isBool()
		|| !patterns.isArray()) {
		return false;
	}
	for (const auto &entry : patterns.toArray()) {
		if (!entry.isString()) {
			return false;
		}
	}
	return !CheckInlinePatterns(Patterns(patterns));
}

} // namespace

QJsonObject InlineDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"rules"_q, QJsonArray() },
	};
}

QJsonObject NewInlineRule(
		const QString &username,
		const QStringList &patterns) {
	return {
		{ u"id"_q,
			QUuid::createUuid().toString(QUuid::WithoutBraces) },
		{ u"username"_q, username },
		{ u"patterns"_q, QJsonArray::fromStringList(patterns) },
		{ u"enabled"_q, false },
	};
}

bool ValidInlineUsername(const QString &username) {
	static const auto expression = QRegularExpression(
		u"\\A[A-Za-z][A-Za-z0-9_]{3,31}\\z"_q);
	return expression.match(username).hasMatch();
}

std::optional<InlineProblem> CheckInlinePatterns(
		const QStringList &patterns) {
	if (patterns.isEmpty() || patterns.size() > kMaxInlinePatterns) {
		return InlineProblem{ 0, u"1-16 patterns required"_q };
	}
	for (auto i = 0; i != patterns.size(); ++i) {
		const auto &pattern = patterns[i];
		if (pattern.isEmpty() || pattern.size() > kMaxInlinePattern
			|| pattern.contains(QChar(u'\n'))
			|| pattern.contains(QChar(u'\r'))
			|| pattern.contains(QChar(0))) {
			return InlineProblem{
				i + 1,
				u"1-512 characters on one line required"_q,
			};
		} else if (const auto problem = Regex::Check(pattern)) {
			return InlineProblem{
				i + 1,
				u"%1 (position %2)"_q.arg(
					problem->text).arg(problem->position),
			};
		}
	}
	return std::nullopt;
}

bool ValidateInlineRules(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	const auto config = Parse(raw);
	if (!config
		|| config->keys() != InlineDefaults().keys()
		|| !config->value(u"version"_q).isDouble()
		|| config->value(u"version"_q).toDouble() != 1.
		|| !config->value(u"rules"_q).isArray()
		|| config->value(u"rules"_q).toArray().size()
			> kMaxInlineRules) {
		return false;
	}
	auto ids = QSet<QString>();
	auto usernames = QSet<QString>();
	for (const auto &entry : config->value(
			u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		const auto id = rule.value(u"id"_q).toString();
		const auto username = rule.value(
			u"username"_q).toString().toLower();
		if (!entry.isObject() || !ValidRule(rule)
			|| ids.contains(id) || usernames.contains(username)) {
			return false;
		}
		ids.insert(id);
		usernames.insert(username);
	}
	return true;
}

QByteArray DisableInlineRules(const QByteArray &raw) {
	if (raw.isEmpty() || !ValidateInlineRules(raw)) {
		return raw;
	}
	auto config = *Parse(raw);
	auto rules = config.value(u"rules"_q).toArray();
	for (auto i = 0; i != rules.size(); ++i) {
		auto rule = rules[i].toObject();
		rule.insert(u"enabled"_q, false);
		rules[i] = rule;
	}
	config.insert(u"rules"_q, rules);
	return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

InlineMatcher CompileInlineRules(const QByteArray &raw) {
	auto result = InlineMatcher();
	if (raw.isEmpty() || !ValidateInlineRules(raw)) {
		return result;
	}
	for (const auto &entry : Parse(raw)->value(
			u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		if (!rule.value(u"enabled"_q).toBool()) {
			continue;
		}
		auto compiled = InlineMatcher::Entry{
			rule.value(u"username"_q).toString(),
		};
		for (const auto &pattern : Patterns(
				rule.value(u"patterns"_q))) {
			compiled.patterns.push_back(Regex::Compile(pattern));
		}
		result.entries.push_back(std::move(compiled));
	}
	return result;
}

QString SingleLink(const QString &text, bool formatted) {
	const auto link = text.trimmed();
	if (formatted || link.size() > kMaxInlineLink
		|| (!link.startsWith(u"http://"_q)
			&& !link.startsWith(u"https://"_q))) {
		return QString();
	}
	for (const auto &ch : link) {
		if (ch.isSpace()) {
			return QString();
		}
	}
	return link;
}

InlineMatch MatchInlineRules(
		const InlineMatcher &matcher,
		const QString &link) {
	auto timer = QElapsedTimer();
	timer.start();
	for (const auto &entry : matcher.entries) {
		for (const auto &pattern : entry.patterns) {
			if (timer.elapsed() >= kMaxInlineWorkMs) {
				return { .error = u"inline rule runtime limit"_q };
			}
			switch (Regex::Find(pattern, link)) {
			case Regex::Found::Yes: return { .username = entry.username };
			case Regex::Found::Limit:
				return { .error = u"inline rule work limit"_q };
			case Regex::Found::No: break;
			}
		}
	}
	return {};
}

InlineMatch AutomaticInlineBot(
		bool enabled,
		const InlineMatcher &matcher,
		const QString &text,
		bool formatted) {
	if (!enabled || matcher.entries.empty()) {
		return {};
	}
	const auto link = SingleLink(text, formatted);
	return link.isEmpty() ? InlineMatch() : MatchInlineRules(matcher, link);
}

} // namespace Nagram::Links
