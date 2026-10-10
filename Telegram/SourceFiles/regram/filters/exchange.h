#pragma once

#include "regram/filters/rules.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace Regram::Filters {

struct ImportResult {
	std::vector<Rule> rules;
	std::vector<Problem> problems;
	QString error;
};

struct MergeResult {
	std::vector<Rule> rules;
	int added = 0;
	int duplicates = 0;
	QString error;
};

struct ExportResult {
	QByteArray data;
	QString error;
};

inline constexpr auto kMaxImportBytes = 8 * 1024 * 1024;

[[nodiscard]] ImportResult ImportRules(const QByteArray &data);
[[nodiscard]] ImportResult ImportStoredRules(const QByteArray &data);
[[nodiscard]] QByteArray ExportStoredRules(const std::vector<Rule> &rules);
[[nodiscard]] ExportResult ExportIosRules(const std::vector<Rule> &rules);
[[nodiscard]] bool IsRulesFileName(const QString &name);
[[nodiscard]] MergeResult MergeRules(
	const std::vector<Rule> &existing,
	const std::vector<Rule> &imported);

} // namespace Regram::Filters
