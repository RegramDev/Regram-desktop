#pragma once

#include "base/assertion.h"
#include "regram/nagram/core/options.h"

#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

#include <vector>

namespace Nagram::Links {

struct InlineMatcher {
	struct Entry {
		QString username;
		std::vector<QRegularExpression> patterns;
	};
	std::vector<Entry> entries;
};

struct InlineMatch {
	QString username;
	QString error;
};

struct InlineProblem {
	int line = 0;
	QString text;
};

inline constexpr auto kMaxInlineLink = 2048;

[[nodiscard]] QJsonObject InlineDefaults();
[[nodiscard]] QJsonObject NewInlineRule(
	const QString &username,
	const QStringList &patterns);
[[nodiscard]] bool ValidateInlineRules(const QByteArray &raw);
[[nodiscard]] bool ValidInlineUsername(const QString &username);
[[nodiscard]] std::optional<InlineProblem> CheckInlinePatterns(
	const QStringList &patterns);
[[nodiscard]] QByteArray DisableInlineRules(const QByteArray &raw);
[[nodiscard]] InlineMatcher CompileInlineRules(const QByteArray &raw);
[[nodiscard]] QString SingleLink(const QString &text, bool formatted);
[[nodiscard]] InlineMatch MatchInlineRules(
	const InlineMatcher &matcher,
	const QString &link);
[[nodiscard]] InlineMatch AutomaticInlineBot(
	bool enabled,
	const InlineMatcher &matcher,
	const QString &text,
	bool formatted);

inline constexpr auto kAutoInlineBot = Option<bool>{
	"nagram.autoInlineBotEnabled", Scope::Device, false,
	Category::Rules, "lng_nagram_inline_auto" };

inline const auto kInlineBotRules = Option<QByteArray>{
	"nagram.inlineBotRules", Scope::Device, QByteArray(),
	Category::Rules, "lng_nagram_inline_rules", 0,
	ValidateInlineRules, DisableInlineRules };

inline void RegisterInlineOptions(Registry &registry) {
	Expects(registry.Add(kAutoInlineBot));
	Expects(registry.Add(kInlineBotRules));
}

} // namespace Nagram::Links
