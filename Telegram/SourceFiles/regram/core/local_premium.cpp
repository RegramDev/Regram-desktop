#include "regram/core/local_premium.h"

#include "base/unixtime.h"
#include "base/weak_ptr.h"
#include "data/data_changes.h"
#include "data/data_emoji_statuses.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "regram/ayu/ayu_settings.h"
#include "storage/storage_account.h"
#include "rpl/event_stream.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <map>

namespace Regram::Premium {
namespace {

constexpr auto kLocalStatusKey = "regram.localEmojiStatus";

auto &Accounts() {
	static auto result = std::map<std::pair<PeerId, bool>, base::weak_ptr<Main::Session>>();
	return result;
}

auto &AccountChanges() {
	static auto result = rpl::event_stream<>();
	return result;
}

Main::Session *AccountFor(not_null<const PeerData*> peer) {
	const auto key = std::pair(peer->id, peer->session().isTestMode());
	const auto found = Accounts().find(key);
	return found == Accounts().end() ? nullptr : found->second.get();
}

struct StatusCache {
	base::weak_ptr<Main::Session> session;
	QByteArray raw;
	std::optional<Data::EmojiStatusData> status;
	base::Timer timer;
	rpl::lifetime lifetime;
};

void NotifyStatus(not_null<StatusCache*> cache) {
	if (const auto session = cache->session.get()) {
		for (const auto &[key, weak] : Accounts()) {
			const auto target = weak.get();
			if (!target || target->isTestMode() != session->isTestMode()) {
				continue;
			}
			if (const auto peer = target->data().peerLoaded(session->user()->id)) {
				target->changes().peerUpdated(peer, Data::PeerUpdate::Flag::EmojiStatus);
			}
		}
	}
}

StatusCache &CacheFor(not_null<Main::Session*> session) {
	static auto caches = std::map<Main::Session*, std::unique_ptr<StatusCache>>();
	auto found = caches.find(session);
	if (found == caches.end() || !found->second->session) {
		std::erase_if(caches, [](const auto &entry) {
			return !entry.second->session;
		});
		auto cache = std::make_unique<StatusCache>();
		cache->session = base::make_weak(session);
		const auto raw = cache.get();
		cache->timer.setCallback([raw] { NotifyStatus(raw); });
		AyuSettings::getInstance().localPremiumChanges()
		| rpl::on_next([raw] { NotifyStatus(raw); }, cache->lifetime);
		found = caches.emplace(session, std::move(cache)).first;
	}
	auto &cache = *found->second;
	const auto raw = session->local().readPref<QByteArray>(kLocalStatusKey);
	if (cache.raw != raw) {
		cache.raw = raw;
		cache.status.reset();
		cache.timer.cancel();
		const auto object = QJsonDocument::fromJson(raw).object();
		auto status = Data::EmojiStatusData();
		status.id.documentId = object.value(u"document"_q).toString().toULongLong();
		status.until = object.value(u"until"_q).toInt();
		const auto collectible = object.value(u"collectible"_q).toObject();
		if (!collectible.isEmpty()) {
			auto value = std::make_shared<Data::EmojiStatusCollectible>();
			value->id = collectible.value(u"id"_q).toString().toULongLong();
			value->documentId = collectible.value(u"document"_q).toString().toULongLong();
			value->patternDocumentId = collectible.value(u"pattern"_q).toString().toULongLong();
			value->title = collectible.value(u"title"_q).toString();
			value->slug = collectible.value(u"slug"_q).toString();
			value->centerColor = QColor(collectible.value(u"center"_q).toString());
			value->edgeColor = QColor(collectible.value(u"edge"_q).toString());
			value->patternColor = QColor(collectible.value(u"patternColor"_q).toString());
			value->textColor = QColor(collectible.value(u"text"_q).toString());
			if (*value) {
				status.id.collectible = std::move(value);
			}
		}
		if (status.id) {
			cache.status = status;
			const auto now = base::unixtime::now();
			if (status.until > now) {
				cache.timer.callOnce(crl::time(status.until - now) * 1000);
			}
		}
	}
	return cache;
}

} // namespace

void RegisterAccount(not_null<Main::Session*> session) {
	std::erase_if(Accounts(), [](const auto &entry) { return !entry.second; });
	const auto key = std::pair(session->user()->id, session->isTestMode());
	Accounts()[key] = base::make_weak(session);
	session->lifetime().add([key, session] {
		const auto found = Accounts().find(key);
		if (found != Accounts().end() && found->second.get() == session.get()) {
			Accounts().erase(found);
			AccountChanges().fire({});
		}
	});
	AccountChanges().fire({});
	NotifyStatus(&CacheFor(session));
}

bool ForLocalFeatures(const Main::Session &session) {
	return session.premium() || AyuSettings::getInstance().localPremium();
}

rpl::producer<bool> ForLocalFeaturesValue(not_null<Main::Session*> session) {
	return rpl::combine(
		Data::PeerPremiumValue(session->user()),
		AyuSettings::getInstance().localPremiumValue(),
		rpl::mappers::_1 || rpl::mappers::_2
	) | rpl::distinct_until_changed();
}

bool ForPeerBadge(not_null<PeerData*> peer) {
	return peer->isPremium()
		|| (AyuSettings::getInstance().localPremium() && AccountFor(peer));
}

rpl::producer<bool> ForPeerBadgeValue(not_null<PeerData*> peer) {
	return rpl::combine(
		Data::PeerPremiumValue(peer),
		AyuSettings::getInstance().localPremiumValue(),
		rpl::single(rpl::empty) | rpl::then(AccountChanges().events())
	) | rpl::map([=](bool premium, bool local, rpl::empty_value) {
		return premium || (local && AccountFor(peer));
	}) | rpl::distinct_until_changed();
}

std::optional<EmojiStatusId> LocalEmojiStatus(not_null<const PeerData*> peer) {
	const auto session = AccountFor(peer);
	if (!session) {
		return std::nullopt;
	}
	const auto &cache = CacheFor(session);
	const auto &status = cache.status;
	if (!AyuSettings::getInstance().localPremium() || session->premium()
		|| !status || (status->until && status->until <= base::unixtime::now())) {
		return std::nullopt;
	}
	return status->id;
}

bool StoreLocalEmojiStatus(
		not_null<PeerData*> peer,
		EmojiStatusId status,
		TimeId until) {
	if (!peer->isSelf() || peer->session().premium()
		|| !AyuSettings::getInstance().localPremium()) {
		return false;
	}
	auto object = QJsonObject{
		{ u"document"_q, QString::number(status.documentId) },
		{ u"until"_q, until },
	};
	if (const auto collectible = status.collectible) {
		object.insert(u"collectible"_q, QJsonObject{
			{ u"id"_q, QString::number(collectible->id) },
			{ u"document"_q, QString::number(collectible->documentId) },
			{ u"pattern"_q, QString::number(collectible->patternDocumentId) },
			{ u"title"_q, collectible->title },
			{ u"slug"_q, collectible->slug },
			{ u"center"_q, collectible->centerColor.name(QColor::HexArgb) },
			{ u"edge"_q, collectible->edgeColor.name(QColor::HexArgb) },
			{ u"patternColor"_q, collectible->patternColor.name(QColor::HexArgb) },
			{ u"text"_q, collectible->textColor.name(QColor::HexArgb) },
		});
	}
	peer->session().local().writePref<QByteArray>(kLocalStatusKey,
		status ? QJsonDocument(object).toJson(QJsonDocument::Compact) : QByteArray());
	NotifyStatus(&CacheFor(&peer->session()));
	return true;
}

} // namespace Regram::Premium
