#include "regram/filters/scopes.h"

#include "regram/filters/rules.h"
#include "base/basic_types.h"

#include <QtCore/QStringList>

#include <algorithm>

namespace Regram::Filters {

ScopeResult ParseScopes(const QString &text) {
	if (text.size() > kMaxScopes * 64) {
		return { .error = u"Scope list exceeds the size limit"_q };
	}
	auto result = ScopeResult();
	const auto entries = QString(text).replace(u'\n', u',').split(u',');
	for (const auto &entry : entries) {
		const auto value = entry.trimmed();
		if (value.isEmpty()) {
			continue;
		}
		const auto parts = value.split(u':');
		if (parts.size() < 2 || parts.size() > 3) {
			return { .error = value };
		}
		const auto name = parts[0].trimmed().toLower();
		const auto kind = (name == u"user"_q) ? std::optional(PeerKind::User)
			: (name == u"group"_q) ? std::optional(PeerKind::Group)
			: (name == u"channel"_q) ? std::optional(PeerKind::Channel)
			: std::nullopt;
		const auto id = ParseServerId(parts[1].trimmed().toStdString());
		const auto thread = (parts.size() == 3)
			? ParseServerId(parts[2].trimmed().toStdString())
			: std::optional<uint64_t>(0);
		if (!kind || !id || !thread
			|| (parts.size() == 3 && *kind != PeerKind::Channel)) {
			return { .error = value };
		}
		result.scopes.push_back({ *kind, *id, *thread });
		if (result.scopes.size() > kMaxScopes) {
			return { .error = value };
		}
	}
	std::sort(result.scopes.begin(), result.scopes.end());
	result.scopes.erase(std::unique(result.scopes.begin(), result.scopes.end()),
		result.scopes.end());
	return result;
}

QString FormatScopes(const std::vector<PeerAddress> &scopes) {
	auto entries = QStringList();
	for (const auto &scope : scopes) {
		const auto name = (scope.kind == PeerKind::User) ? u"user"_q
			: (scope.kind == PeerKind::Group) ? u"group"_q : u"channel"_q;
		auto entry = name + u':' + QString::number(scope.id);
		if (scope.thread) {
			entry += u':' + QString::number(scope.thread);
		}
		entries.push_back(std::move(entry));
	}
	return entries.join(u'\n');
}

} // namespace Regram::Filters
