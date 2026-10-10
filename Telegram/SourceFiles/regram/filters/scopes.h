#pragma once

#include "regram/core/peer_address.h"

#include <QtCore/QString>

#include <vector>

namespace Regram::Filters {

struct ScopeResult {
	std::vector<PeerAddress> scopes;
	QString error;
};

[[nodiscard]] ScopeResult ParseScopes(const QString &text);
[[nodiscard]] QString FormatScopes(const std::vector<PeerAddress> &scopes);

} // namespace Regram::Filters
