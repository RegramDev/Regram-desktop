#include "regram/nagram/filters/view.h"

#include "regram/filters/runtime.h"
#include "regram/filters/reveal.h"
#include "regram/filters/search_text.h"

#include "regram/nagram/filters/hidden_messages.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_groups.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QCache>
#include <QtCore/QHash>

#include <array>

namespace Nagram::Filters {
namespace {

struct Cached {
	FullMsgId id;
	QByteArray config;
	TextWithEntities source;
	QString searchable;
	QString author;
	QString peer;
	bool blocked = false;
	bool outgoing = false;
	Result result;
};

QCache<quintptr, Cached> &Results() {
	static auto result = QCache<quintptr, Cached>(4096);
	return result;
}

QString Searchable(not_null<HistoryItem*> item) {
	return Regram::Filters::SearchText(item);
}

QJsonObject ConfigFor(const QByteArray &raw) {
	static auto cache = QCache<QByteArray, QJsonObject>(32);
	if (const auto found = cache.object(raw)) {
		return *found;
	}
	const auto parsed = QJsonDocument::fromJson(raw).object();
	cache.insert(raw, new QJsonObject(parsed));
	return parsed;
}

struct ResolvedCache {
	Main::Session *session = nullptr;
	QByteArray account;
	QByteArray global;
	QByteArray scopes;
	QHash<QString, Resolved> places;
};

Resolved ResolveFor(
		not_null<Main::Session*> session,
		const QString &peer,
		const QString &topic) {
	static auto cache = ResolvedCache();
	const auto account = ForAccount(session).Get(kRules);
	const auto global = ForDevice().Get(kGlobalRules);
	const auto scopes = ForAccount(session).Get(kScopes);
	if (global.isEmpty() && scopes.isEmpty()) {
		return { .config = account };
	}
	if (cache.session != session
		|| cache.account != account
		|| cache.global != global
		|| cache.scopes != scopes) {
		cache = { session, account, global, scopes };
	}
	const auto place = peer + u':' + topic;
	const auto found = cache.places.constFind(place);
	if (found != cache.places.cend()) {
		return *found;
	}
	const auto result = Resolve(account, global, scopes, peer, topic);
	if (!result.error.isEmpty()) {
		LOG(("Nagram filter: %1 (chat %2, topic %3).")
			.arg(result.error, peer, topic));
	}
	if (cache.places.size() >= 512) {
		cache.places.clear();
	}
	cache.places.insert(place, result);
	return result;
}

} // namespace

Result Project(HistoryItem *item, const TextWithEntities &source, bool displayOverrides) {
	if (!item || item->isService()) {
		return { .text = source };
	}
	if (displayOverrides && (item->nagramOriginalShown()
		|| Regram::Filters::TemporarilyRevealed(item->history()->peer))) {
		return { .text = source };
	}
	if (MessageHidden(item) || Regram::Filters::Hidden(item)) {
		return { .text = source, .hidden = true };
	}
	const auto peer = item->history()->peer;
	const auto peerId = QString::number(SerializePeerId(peer->id));
	const auto resolved = ResolveFor(
		&item->history()->session(),
		peerId,
		QString::number(peer->isForum() ? item->topicRootId().bare : 0));
	const auto &raw = resolved.config;
	if (!resolved.error.isEmpty()) {
		return { .text = source, .error = resolved.error };
	} else if (raw.isEmpty()) {
		return { .text = source };
	}
	const auto config = ConfigFor(raw);
	if (!config.value(u"enabled"_q).toBool()
		|| config.value(u"excludedPeers"_q).toArray().contains(peerId)
		|| (item->out() && !config.value(u"filterOutgoing"_q).toBool())) {
		return { .text = source };
	}
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	const auto sources = std::array<PeerData*, 3>{
		item->from().get(),
		forwarded ? forwarded->originalSender : nullptr,
		item->viaBot(),
	};
	const auto hiddenAuthors = config.value(u"hiddenAuthors"_q).toArray();
	auto author = QString();
	auto blocked = false;
	for (const auto &source : sources) {
		if (!source) {
			continue;
		}
		blocked = blocked || source->isBlocked();
		const auto id = QString::number(SerializePeerId(source->id));
		if (hiddenAuthors.contains(id)) {
			author = id;
		}
	}
	const auto searchable = Searchable(item);
	const auto key = reinterpret_cast<quintptr>(item);
	if (const auto cached = Results().object(key);
		cached && cached->id == item->fullId()
		&& cached->config == raw
		&& cached->source.text == source.text
		&& cached->source.entities == source.entities
		&& cached->searchable == searchable
		&& cached->author == author && cached->peer == peerId
		&& cached->blocked == blocked && cached->outgoing == item->out()) {
		return cached->result;
	}
	const auto result = Apply(raw, source, author,
		peerId, blocked, item->out(), searchable);
	Results().insert(key, new Cached{
		.id = item->fullId(),
		.config = raw,
		.source = source,
		.searchable = searchable,
		.author = author,
		.peer = peerId,
		.blocked = blocked,
		.outgoing = item->out(),
		.result = result,
	});
	if (!result.error.isEmpty()) {
		LOG(("Nagram filter: %1 (message %2).")
			.arg(result.error, QString::number(item->id.bare)));
	}
	return result;
}

bool Hidden(HistoryItem *item) {
	return item && Project(item,
		item->translatedTextWithLocalEntities()).hidden;
}

bool HiddenForNotification(HistoryItem *item) {
	return item && Project(item, item->originalText(), false).hidden;
}

TextWithEntities DisplayText(const Result &result) {
	return result.hidden
		? TextWithEntities{ tr::lng_nagram_filter_hidden(tr::now) }
		: result.text;
}

} // namespace Nagram::Filters
