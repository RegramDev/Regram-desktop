#pragma once

#include "regram/filters/exchange.h"
#include "regram/nagram/core/options.h"

class HistoryItem;
class PeerData;

namespace Regram::Filters {

struct StoredRules {
	QByteArray raw;
	std::vector<Rule> rules;
	std::vector<Problem> problems;
	QString error;
};

[[nodiscard]] bool ValidateStored(const QByteArray &raw);
[[nodiscard]] StoredRules Read(not_null<Main::Session*> session);
void Migrate(not_null<Main::Session*> session);
[[nodiscard]] QString Write(
	not_null<Main::Session*> session,
	const QByteArray &expected,
	const std::vector<Rule> &rules);
[[nodiscard]] bool Hidden(
	not_null<HistoryItem*> item,
	bool displayOverrides = true);
[[nodiscard]] bool SuppressNotification(not_null<HistoryItem*> item);
[[nodiscard]] rpl::producer<> Updates(not_null<Main::Session*> session);
void NotifyChanged(not_null<Main::Session*> session);

[[nodiscard]] std::vector<uint64> HiddenSenders(
	not_null<Main::Session*> session);
[[nodiscard]] bool SenderHidden(not_null<PeerData*> peer);
[[nodiscard]] bool SetSenderHidden(not_null<PeerData*> peer, bool hidden);

[[nodiscard]] bool ValidDisabledChats(const QByteArray &raw);
[[nodiscard]] bool EnabledForPeer(not_null<PeerData*> peer);
[[nodiscard]] bool SetEnabledForPeer(not_null<PeerData*> peer, bool enabled);

inline const auto kDisabledChats = Nagram::Option<QByteArray>{
	"nagram.regramFilterDisabledChats", Nagram::Scope::Account, QByteArray(),
	Nagram::Category::Rules, "lng_regram_filter_enabled_chat",
	static_cast<unsigned>(Nagram::Flag::RefreshMessageView)
		| static_cast<unsigned>(Nagram::Flag::RefreshDialogList), ValidDisabledChats };

inline const auto kHiddenSenders = Nagram::Option<QByteArray>{
	"nagram.regramHiddenSenders", Nagram::Scope::Account, QByteArray(),
	Nagram::Category::Rules, "lng_regram_filter_hidden_senders",
	static_cast<unsigned>(Nagram::Flag::RefreshMessageView)
		| static_cast<unsigned>(Nagram::Flag::RefreshDialogList), ValidDisabledChats };

inline const auto kSuppressNotifications = Nagram::Option<bool>{
	"nagram.regramSuppressFilteredNotifications", Nagram::Scope::Account, false,
	Nagram::Category::Rules, "lng_regram_filter_mute_notifications" };

inline const auto kPortableRules = Nagram::Option<QByteArray>{
	"nagram.regramIosFilters", Nagram::Scope::Account, QByteArray(),
	Nagram::Category::Rules, "lng_regram_ios_filters",
	static_cast<unsigned>(Nagram::Flag::RefreshMessageView)
		| static_cast<unsigned>(Nagram::Flag::RefreshDialogList), ValidateStored };

} // namespace Regram::Filters
