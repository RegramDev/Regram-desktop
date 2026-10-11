#pragma once

#include "base/basic_types.h"
#include "regram/nagram/core/options.h"

#include <optional>
#include <vector>

namespace Nagram::Notifications {

inline constexpr auto kMinutesPerDay = 24 * 60;
inline constexpr auto kMaxKeywordRules = 50;
inline constexpr auto kMaxKeywordLength = 256;

struct QuietHours {
	bool enabled = false;
	int start = 23 * 60;
	int end = 7 * 60;
	std::vector<int> weekdays;
	bool allowContacts = false;
	bool allowPinned = false;
	bool allowMentions = false;
	bool allowKeywords = false;

	friend bool operator==(const QuietHours &, const QuietHours &) = default;
};

struct Facts {
	bool message = false;
	bool contact = false;
	bool pinned = false;
	bool mention = false;
	bool keyword = false;
};

struct KeywordRule {
	QString pattern;
	bool regex = false;
	bool caseSensitive = false;

	friend bool operator==(const KeywordRule &, const KeywordRule &) = default;
};

struct KeywordAlerts {
	bool enabled = false;
	bool channels = false;
	std::vector<KeywordRule> rules;

	friend bool operator==(
		const KeywordAlerts &,
		const KeywordAlerts &) = default;
};

struct KeywordProblem {
	int line = 0;
	QString text;
};

[[nodiscard]] std::optional<QuietHours> ParseQuietHours(const QByteArray &raw);
[[nodiscard]] QByteArray Serialize(const QuietHours &value);
[[nodiscard]] bool ValidQuietHours(const QByteArray &raw);
[[nodiscard]] std::optional<int> ParseMinute(const QString &text);
[[nodiscard]] QString FormatMinute(int minute);
[[nodiscard]] bool Active(const QuietHours &config, int weekday, int minute);
[[nodiscard]] bool Silences(
	const QuietHours &config,
	const Facts &facts,
	int weekday,
	int minute);

[[nodiscard]] std::optional<KeywordAlerts> ParseKeywordAlerts(
	const QByteArray &raw);
[[nodiscard]] QByteArray Serialize(const KeywordAlerts &value);
[[nodiscard]] bool ValidKeywordAlerts(const QByteArray &raw);
[[nodiscard]] std::vector<KeywordRule> ParseKeywordLines(const QString &text);
[[nodiscard]] QString FormatKeywordLines(
	const std::vector<KeywordRule> &rules);
[[nodiscard]] std::optional<KeywordProblem> CheckKeywordRules(
	const std::vector<KeywordRule> &rules);
[[nodiscard]] bool Matches(
	const KeywordAlerts &config,
	const QString &text,
	bool channel);

inline const auto kQuietHours = Option<QByteArray>{
	"nagram.quietHours", Scope::Device, QByteArray(),
	Category::Interface, "lng_nagram_quiet_hours", 0,
	ValidQuietHours };
inline const auto kKeywordAlerts = Option<QByteArray>{
	"nagram.keywordAlerts", Scope::Account, QByteArray(),
	Category::Rules, "lng_nagram_keyword_alerts", 0,
	ValidKeywordAlerts };

[[nodiscard]] bool ValidPolicy(const QString &value);
[[nodiscard]] bool ValidPolicyExceptions(const QByteArray &raw);
[[nodiscard]] QString PolicyForPeer(
	const QString &fallback,
	const QByteArray &exceptions,
	const QString &peer);

[[nodiscard]] QString PolicyForPeer(
	const QString &fallback,
	const QJsonObject &exceptions,
	const QString &peer);

inline const auto kPinnedPolicy = Option<QString>{
	"nagram.regramPinnedNotifications", Scope::Account, u"default"_q,
	Category::Interface, "lng_regram_pinned_notifications", 0, ValidPolicy };
inline const auto kMentionPolicy = Option<QString>{
	"nagram.regramMentionNotifications", Scope::Account, u"default"_q,
	Category::Interface, "lng_regram_mention_notifications", 0, ValidPolicy };
inline const auto kPinnedExceptions = Option<QByteArray>{
	"nagram.regramPinnedNotificationExceptions", Scope::Account, QByteArray(),
	Category::Interface, "lng_regram_pinned_notifications",
	static_cast<unsigned>(Flag::Hidden), ValidPolicyExceptions };
inline const auto kMentionExceptions = Option<QByteArray>{
	"nagram.regramMentionNotificationExceptions", Scope::Account, QByteArray(),
	Category::Interface, "lng_regram_mention_notifications",
	static_cast<unsigned>(Flag::Hidden), ValidPolicyExceptions };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kQuietHours));
	Expects(registry.Add(kPinnedPolicy));
	Expects(registry.Add(kMentionPolicy));
	Expects(registry.Add(kPinnedExceptions));
	Expects(registry.Add(kMentionExceptions));
	Expects(registry.Add(kKeywordAlerts));
}

} // namespace Nagram::Notifications
