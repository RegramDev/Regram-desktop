#pragma once

#include "regram/nagram/services/model.h"

namespace Nagram {

inline constexpr auto kSummaryMessages = 50;
inline constexpr auto kSummaryLength = 24000;
inline constexpr auto kSummaryResultLength = 32768;

struct SummaryScope {
	QStringList texts;
	int characters = 0;
	bool truncated = false;
};

[[nodiscard]] SummaryScope PlanSummary(const QStringList &texts);
[[nodiscard]] QJsonObject BuildSummaryBody(
	const ServiceDefinition &service,
	const QStringList &texts,
	const QString &language);
[[nodiscard]] std::optional<QString> ParseSummaryResult(
	const ServiceDefinition &service,
	const QByteArray &body);

} // namespace Nagram
