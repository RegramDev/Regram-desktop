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

[[nodiscard]] ImportResult ImportRules(const QByteArray &data);
[[nodiscard]] QByteArray ExportRules(const std::vector<Rule> &rules);
[[nodiscard]] MergeResult MergeRules(
	const std::vector<Rule> &existing,
	const std::vector<Rule> &imported);

} // namespace Regram::Filters
