#include "regram/nagram/links/model.h"
#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QUuid>

namespace Nagram::Links {
namespace {

bool Host(const QJsonValue &value, bool allowEmpty = false) {
	if (!value.isString()) {
		return false;
	}
	const auto host = value.toString();
	if (host.isEmpty()) {
		return allowEmpty;
	} else if (host.size() > 253 || host != host.toLower()) {
		return false;
	}
	static const auto label = QRegularExpression(
		u"\\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\z"_q);
	for (const auto &part : host.split('.')) {
		if (!label.match(part).hasMatch()) {
			return false;
		}
	}
	return true;
}

bool ValidObject(const QJsonObject &config) {
	if (config.keys() != Defaults().keys()
		|| config.value(u"version"_q) != 1
		|| !config.value(u"confirmAll"_q).isBool()
		|| !config.value(u"rules"_q).isArray()
		|| config.value(u"rules"_q).toArray().size() > 32) {
		return false;
	}
	auto ids = QSet<QString>();
	for (const auto &entry : config.value(u"rules"_q).toArray()) {
		if (!entry.isObject()) {
			return false;
		}
		const auto rule = entry.toObject();
		const auto id = rule.value(u"id"_q).toString();
		const auto uuid = QUuid(id);
		if (rule.size() != 5 || uuid.isNull()
			|| uuid.toString(QUuid::WithoutBraces) != id
			|| ids.contains(id)
			|| !rule.value(u"enabled"_q).isBool()
			|| !Host(rule.value(u"host"_q))
			|| !Host(rule.value(u"replacementHost"_q), true)
			|| !rule.value(u"removeParameters"_q).isArray()) {
			return false;
		}
		const auto parameters = rule.value(u"removeParameters"_q).toArray();
		if (parameters.size() > 32
			|| (parameters.isEmpty()
				&& rule.value(u"replacementHost"_q).toString().isEmpty())) {
			return false;
		}
		auto names = QSet<QString>();
		static const auto pattern = QRegularExpression(
			u"\\A[a-zA-Z0-9_+.-]{1,64}\\*?\\z"_q);
		for (const auto &parameter : parameters) {
			const auto name = parameter.toString();
			if (!parameter.isString() || !pattern.match(name).hasMatch()
				|| names.contains(name)) {
				return false;
			}
			names.insert(name);
		}
		ids.insert(id);
	}
	return true;
}

bool Removes(const QString &name, const QJsonArray &patterns) {
	for (const auto &value : patterns) {
		const auto pattern = value.toString();
		if (pattern.endsWith('*')
			? name.startsWith(pattern.chopped(1)) : name == pattern) {
			return true;
		}
	}
	return false;
}

} // namespace

QJsonObject Defaults() {
	return {
		{ u"version"_q, 1 },
		{ u"confirmAll"_q, false },
		{ u"rules"_q, QJsonArray() },
	};
}

bool Validate(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	return error.error == QJsonParseError::NoError
		&& document.isObject() && ValidObject(document.object());
}

QJsonObject NewRule(
		const QString &host,
		const QString &replacementHost,
		const QStringList &removeParameters) {
	auto parameters = QJsonArray();
	for (const auto &name : removeParameters) {
		parameters.push_back(name);
	}
	return {
		{ u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces) },
		{ u"host"_q, host },
		{ u"replacementHost"_q, replacementHost },
		{ u"removeParameters"_q, parameters },
		{ u"enabled"_q, false },
	};
}

Result Rewrite(const QByteArray &raw, const QString &original) {
	auto result = Result();
	if (!Validate(raw) || original.size() > 16384) {
		result.error = u"invalid link rule configuration"_q;
		return result;
	}
	result.url = QUrl(original, QUrl::StrictMode);
	if (!result.url.isValid() || result.url.host().isEmpty()
		|| (result.url.scheme() != u"https"_q
			&& result.url.scheme() != u"http"_q)) {
		result.error = u"invalid external URL"_q;
		return result;
	}
	const auto originalUrl = result.url;
	const auto config = raw.isEmpty()
		? Defaults() : QJsonDocument::fromJson(raw).object();
	for (const auto &entry : config.value(u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		if (!rule.value(u"enabled"_q).toBool()
			|| QString::fromLatin1(QUrl::toAce(result.url.host())).toLower()
				!= rule.value(u"host"_q).toString()) {
			continue;
		}
		if (!result.url.userInfo().isEmpty()) {
			result.error = u"link contains credentials"_q;
			return result;
		}
		const auto host = rule.value(u"replacementHost"_q).toString();
		if (!host.isEmpty() && host != result.url.host()) {
			result.url.setScheme(u"https"_q);
			result.url.setHost(host);
			result.url.setPort(-1);
		}
		const auto parameters = rule.value(u"removeParameters"_q).toArray();
		if (!parameters.isEmpty() && result.url.hasQuery()) {
			auto kept = QStringList();
			for (const auto &part : result.url.query(
					QUrl::FullyEncoded).split('&')) {
				const auto name = QUrl::fromPercentEncoding(
					part.section('=', 0, 0).toUtf8());
				if (!Removes(name, parameters)) {
					kept.push_back(part);
				}
			}
			result.url.setQuery(kept.isEmpty() ? QString() : kept.join('&'),
				QUrl::StrictMode);
		}
		break;
	}
	result.changed = result.url != originalUrl;
	return result;
}

} // namespace Nagram::Links
