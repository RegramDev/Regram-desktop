#pragma once

#include "base/assertion.h"
#include "regram/nagram/core/options.h"
#include "regram/nagram/core/regex.h"

namespace Nagram::Links {

struct WebviewDecision {
	bool open = false;
	QString error;
};

inline constexpr auto kMaxWebviewPattern = 2048;
inline constexpr auto kWebviewOpenInterval = 1000;

[[nodiscard]] std::optional<Regex::Problem> CheckWebviewPattern(
	const QString &pattern);
[[nodiscard]] bool ValidWebviewPattern(const QString &pattern);
[[nodiscard]] QRegularExpression CompileWebviewPattern(
	const QString &pattern);
[[nodiscard]] WebviewDecision MatchOutsideWebview(
	const QRegularExpression &pattern,
	const QString &uri,
	const QString &startUrl);
[[nodiscard]] bool AllowOutsideOpen(qint64 &last, qint64 now);

inline const auto kWebviewExternalPattern = Option<QString>{
	"nagram.webviewExternalPattern", Scope::Device, QString(),
	Category::Rules, "lng_nagram_webview_external", 0,
	ValidWebviewPattern };

inline void RegisterWebviewOptions(Registry &registry) {
	Expects(registry.Add(kWebviewExternalPattern));
}

} // namespace Nagram::Links
