#pragma once

#include "regram/nagram/core/options.h"

#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace Nagram::Links {

struct Result {
	QUrl url;
	QString error;
	bool changed = false;
};

[[nodiscard]] QJsonObject Defaults();
[[nodiscard]] bool Validate(const QByteArray &raw);
[[nodiscard]] QJsonObject NewRule(
	const QString &host,
	const QString &replacementHost,
	const QStringList &removeParameters);
[[nodiscard]] Result Rewrite(const QByteArray &raw, const QString &original);

inline const auto kRules = Option<QByteArray>{
	"nagram.linkRules", Scope::Device, QByteArray(),
	Category::Rules, "lng_nagram_link_rules", 0, Validate };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kRules));
}

} // namespace Nagram::Links
