#include "regram/nagram/interface/notifications.h"

#include "regram/nagram/chats/local_pins.h"
#include "regram/nagram/notifications/model.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"

#include <array>
#include <map>

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

QString SpecialPolicy(not_null<HistoryItem*> item, bool message) {
	if (!message) {
		return u"default"_q;
	}
	const auto pinned = item->Has<HistoryServicePinned>();
	if (!pinned && !item->mentionsMe()) {
		return u"default"_q;
	}
	struct Snapshot {
		QByteArray raw;
		QJsonObject values;
	};
	struct Cache {
		base::weak_ptr<Main::Session> session;
		std::array<Snapshot, 2> exceptions;
	};
	static auto caches = std::map<Main::Session*, Cache>();
	std::erase_if(caches, [](const auto &entry) { return !entry.second.session; });
	const auto session = &item->history()->session();
	auto &cache = caches[session];
	cache.session = base::make_weak(session);
	auto &options = ForAccount(session);
	auto &snapshot = cache.exceptions[pinned ? 0 : 1];
	const auto raw = options.Get(pinned ? kPinnedExceptions : kMentionExceptions);
	if (snapshot.raw != raw) {
		snapshot.raw = raw;
		snapshot.values = QJsonDocument::fromJson(raw).object();
	}
	return PolicyForPeer(
		options.Get(pinned ? kPinnedPolicy : kMentionPolicy),
		snapshot.values,
		QString::number(SerializePeerId(item->history()->peer->id)));
}

} // namespace

std::optional<bool> ReviewNotification(
		not_null<HistoryItem*> item,
		bool message) {
	if (SpecialPolicy(item, message) == u"disabled"_q) {
		return false;
	}
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
			.keyword = false,
		}, now.date().dayOfWeek(), now.time().msecsSinceStartOfDay() / 60'000);
		if (silenced) {
			return false;
		}
	}
	return std::nullopt;
}

bool SilentNotification(not_null<HistoryItem*> item, bool message) {
	return SpecialPolicy(item, message) == u"silenced"_q;
}

} // namespace Nagram::Interface
