#pragma once

#include "base/assertion.h"
#include "base/basic_types.h"
#include "ui/text/text_entity.h"
#include "regram/nagram/core/options.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>

#include <vector>

namespace Nagram::Filters {

enum class Layer { Global, Account, Chat };

struct Result {
	TextWithEntities text;
	bool hidden = false;
	QString error;
	int matches = 0;
};

struct Resolved {
	QByteArray config;
	QString error;
	int count = 0;
};

struct InheritedRule {
	QJsonObject rule;
	Layer layer = Layer::Global;
};

struct InheritedState {
	bool enabled = false;
	Layer layer = Layer::Account;
};

inline constexpr auto kMaxRules = 32;

[[nodiscard]] QJsonObject Defaults();
[[nodiscard]] QJsonObject GlobalDefaults();
[[nodiscard]] QJsonObject ScopeDefaults();
[[nodiscard]] QJsonObject NewScope(const QString &peer, const QString &topic);
[[nodiscard]] bool DefaultScope(const QJsonObject &scope);
[[nodiscard]] bool Validate(const QByteArray &raw);
[[nodiscard]] bool ValidateGlobal(const QByteArray &raw);
[[nodiscard]] bool ValidateScopes(const QByteArray &raw);
[[nodiscard]] Resolved Resolve(
	const QByteArray &account,
	const QByteArray &global,
	const QByteArray &scopes,
	const QString &peer,
	const QString &topic);
[[nodiscard]] std::vector<InheritedRule> InheritedRules(
	const QByteArray &account,
	const QByteArray &global,
	const QByteArray &scopes,
	const QString &peer,
	const QString &topic);
[[nodiscard]] InheritedState InheritedEnabled(
	const QByteArray &account,
	const QByteArray &scopes,
	const QString &peer,
	const QString &topic);
[[nodiscard]] Result Apply(
	const QByteArray &raw,
	const TextWithEntities &source,
	const QString &author,
	const QString &peer,
	bool blocked,
	bool outgoing,
	const QString &searchable = QString());

inline const auto kRules = Option<QByteArray>{
	"nagram.filters", Scope::Account, QByteArray(),
	Category::Rules, "lng_nagram_filter_rules",
	static_cast<unsigned>(Flag::RefreshMessageView), Validate };

inline const auto kGlobalRules = Option<QByteArray>{
	"nagram.filtersGlobal", Scope::Device, QByteArray(),
	Category::Rules, "lng_nagram_filter_global",
	static_cast<unsigned>(Flag::RefreshMessageView), ValidateGlobal };

inline const auto kScopes = Option<QByteArray>{
	"nagram.filterScopes", Scope::Account, QByteArray(),
	Category::Rules, "lng_nagram_filter_scopes",
	static_cast<unsigned>(Flag::RefreshMessageView), ValidateScopes };

inline constexpr auto kMaximumHiddenMessages = 1000;

[[nodiscard]] inline bool ValidHiddenMessages(const QString &value) {
	if (value.isEmpty()) {
		return true;
	}
	const auto entries = value.split(u',');
	if (entries.size() > kMaximumHiddenMessages) {
		return false;
	}
	for (const auto &entry : entries) {
		const auto parts = entry.split(u':');
		auto peerOk = false;
		auto msgOk = false;
		if (parts.size() != 2
			|| !parts[0].toULongLong(&peerOk) || !peerOk
			|| parts[1].toLongLong(&msgOk) <= 0 || !msgOk) {
			return false;
		}
	}
	return true;
}

inline const auto kHiddenMessages = Option<QString>{
	"nagram.hiddenMessages", Scope::Account, QString(),
	Category::Rules, "lng_nagram_hidden_messages",
	static_cast<unsigned>(Flag::RefreshMessageView), ValidHiddenMessages };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kRules));
	Expects(registry.Add(kGlobalRules));
	Expects(registry.Add(kScopes));
	Expects(registry.Add(kHiddenMessages));
}

} // namespace Nagram::Filters
