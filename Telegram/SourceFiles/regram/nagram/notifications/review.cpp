#include "regram/nagram/interface/notifications.h"

#include "regram/nagram/chats/local_pins.h"
#include "regram/nagram/filters/view.h"
#include "regram/nagram/notifications/model.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

#include <QtCore/QDateTime>

namespace Nagram::Interface {
namespace {

using namespace Notifications;

template <typename Document>
[[nodiscard]] const Document &Cached(
		const QByteArray &raw,
		std::optional<Document> (*parse)(const QByteArray &)) {
	static auto last = QByteArray();
	static auto parsed = Document();
	if (raw != last) {
		parsed = parse(raw).value_or(Document());
		last = raw;
	}
	return parsed;
}

[[nodiscard]] bool KeywordMatch(not_null<HistoryItem*> item, bool message) {
	if (!message || item->out() || item->isService()) {
		return false;
	}
	const auto history = item->history();
	const auto raw = ForAccount(&history->session()).Get(kKeywordAlerts);
	const auto &config = Cached(raw, ParseKeywordAlerts);
	return config.enabled
		&& !item->from()->isBlocked()
		&& Matches(
			config,
			item->originalText().text,
			history->peer->isBroadcast())
		&& !Filters::Hidden(item);
}

} // namespace

std::optional<bool> ReviewNotification(
		not_null<HistoryItem*> item,
		bool message) {
	const auto keyword = KeywordMatch(item, message);
	const auto raw = ForDevice().Get(kQuietHours);
	const auto &quiet = Cached(raw, ParseQuietHours);
	if (quiet.enabled) {
		const auto history = item->history();
		const auto user = history->peer->asUser();
		const auto now = QDateTime::currentDateTime();
		const auto silenced = Silences(quiet, {
			.message = message,
			.contact = user && user->isContact(),
			.pinned = Chats::ShowsPinnedIcon(history, FilterId()),
			.mention = message && item->mentionsMe(),
			.keyword = keyword,
		}, now.date().dayOfWeek(), now.time().msecsSinceStartOfDay() / 60'000);
		if (silenced) {
			return false;
		}
	}
	return keyword ? std::make_optional(true) : std::nullopt;
}

} // namespace Nagram::Interface
