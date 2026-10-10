#include "regram/nagram/links/webview.h"

#include "base/basic_types.h"

#include <QtCore/QUrl>

namespace Nagram::Links {
namespace {

constexpr auto kMaxWebviewUri = 8192;
constexpr auto kOptions = QRegularExpression::CaseInsensitiveOption
	| QRegularExpression::MultilineOption;

[[nodiscard]] QString WithoutFragment(const QString &uri) {
	return QUrl(uri).adjusted(QUrl::RemoveFragment).toString(
		QUrl::FullyEncoded);
}

} // namespace

std::optional<Regex::Problem> CheckWebviewPattern(const QString &pattern) {
	if (pattern.isEmpty()) {
		return std::nullopt;
	} else if (pattern.size() > kMaxWebviewPattern) {
		return Regex::Problem{ kMaxWebviewPattern, u"pattern is too long"_q };
	}
	return Regex::Check(pattern, kOptions);
}

bool ValidWebviewPattern(const QString &pattern) {
	return !CheckWebviewPattern(pattern);
}

QRegularExpression CompileWebviewPattern(const QString &pattern) {
	return Regex::Compile(pattern, kOptions);
}

WebviewDecision MatchOutsideWebview(
		const QRegularExpression &pattern,
		const QString &uri,
		const QString &startUrl) {
	const auto parsed = QUrl(uri);
	const auto scheme = parsed.scheme().toLower();
	if (uri.size() > kMaxWebviewUri
		|| !parsed.isValid()
		|| (scheme != u"http"_q && scheme != u"https"_q)
		|| uri.contains(u"tgWebAppData"_q, Qt::CaseInsensitive)
		|| WithoutFragment(uri) == WithoutFragment(startUrl)) {
		return {};
	}
	switch (Regex::Find(pattern, uri)) {
	case Regex::Found::Yes: return { .open = true };
	case Regex::Found::Limit:
		return { .error = u"web app link pattern work limit"_q };
	case Regex::Found::No: break;
	}
	return {};
}

bool AllowOutsideOpen(qint64 &last, qint64 now) {
	if (last && now - last < kWebviewOpenInterval) {
		return false;
	}
	last = now;
	return true;
}

} // namespace Nagram::Links
