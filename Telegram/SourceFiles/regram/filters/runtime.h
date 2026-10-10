#pragma once

#include "regram/filters/exchange.h"
#include "regram/nagram/core/options.h"

class HistoryItem;

namespace Regram::Filters {

struct StoredRules {
	QByteArray raw;
	std::vector<Rule> rules;
	std::vector<Problem> problems;
	QString error;
};

[[nodiscard]] bool ValidateStored(const QByteArray &raw);
[[nodiscard]] StoredRules Read(not_null<Main::Session*> session);
[[nodiscard]] QString Write(
	not_null<Main::Session*> session,
	const QByteArray &expected,
	const std::vector<Rule> &rules);
[[nodiscard]] bool Hidden(not_null<HistoryItem*> item);
[[nodiscard]] bool SuppressNotification(not_null<HistoryItem*> item);

inline const auto kSuppressNotifications = Nagram::Option<bool>{
	"nagram.regramSuppressFilteredNotifications", Nagram::Scope::Account, false,
	Nagram::Category::Rules, "lng_regram_filter_mute_notifications" };

inline const auto kPortableRules = Nagram::Option<QByteArray>{
	"nagram.regramIosFilters", Nagram::Scope::Account, QByteArray(),
	Nagram::Category::Rules, "lng_regram_ios_filters",
	static_cast<unsigned>(Nagram::Flag::RefreshMessageView)
		| static_cast<unsigned>(Nagram::Flag::RefreshDialogList), ValidateStored };

} // namespace Regram::Filters
