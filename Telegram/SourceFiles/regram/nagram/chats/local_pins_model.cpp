#include "regram/nagram/chats/local_pins_model.h"

#include "regram/nagram/core/owned_json.h"

#include <QtCore/QJsonArray>

#include <algorithm>

namespace Nagram::Chats {
namespace {

[[nodiscard]] std::optional<std::vector<quint64>> ParsePeers(
		const QJsonObject &object) {
	const auto value = object.value(QStringLiteral("peers"));
	if (!value.isArray() || value.toArray().size() > kLocalPinsLimit) {
		return std::nullopt;
	}
	auto result = std::vector<quint64>();
	for (const auto &entry : value.toArray()) {
		const auto id = DecimalId(entry);
		if (!id || std::find(result.begin(), result.end(), *id)
			!= result.end()) {
			return std::nullopt;
		}
		result.push_back(*id);
	}
	return result;
}

[[nodiscard]] std::optional<QJsonObject> ParseObject(const QByteArray &raw) {
	return ParseOwnedJson(raw, {
		QStringLiteral("version"),
		QStringLiteral("user"),
		QStringLiteral("peers"),
	});
}

} // namespace

bool ValidLocalPins(const QByteArray &raw) {
	const auto object = ParseObject(raw);
	return object && ParsePeers(*object).has_value();
}

std::vector<quint64> ParseLocalPins(const QByteArray &raw, quint64 user) {
	const auto object = ParseObject(raw);
	if (!object || !OwnedBy(*object, user)) {
		return {};
	}
	return ParsePeers(*object).value_or(std::vector<quint64>());
}

QByteArray SerializeLocalPins(
		const std::vector<quint64> &peers,
		quint64 user) {
	if (peers.empty()) {
		return QByteArray();
	}
	auto list = QJsonArray();
	for (const auto &peer : peers) {
		list.push_back(QString::number(peer));
	}
	auto object = NewOwnedJson(user);
	object.insert(QStringLiteral("peers"), list);
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool AddLocalPin(std::vector<quint64> &peers, quint64 peer) {
	const auto i = std::find(peers.begin(), peers.end(), peer);
	if (i != peers.end()) {
		std::rotate(peers.begin(), i, i + 1);
		return true;
	} else if (!peer || int(peers.size()) >= kLocalPinsLimit) {
		return false;
	}
	peers.insert(peers.begin(), peer);
	return true;
}

bool RemoveLocalPin(std::vector<quint64> &peers, quint64 peer) {
	const auto i = std::find(peers.begin(), peers.end(), peer);
	if (i == peers.end()) {
		return false;
	}
	peers.erase(i);
	return true;
}

bool MergeLocalPins(
		std::vector<quint64> &peers,
		const std::vector<quint64> &serverPinned) {
	const auto was = peers.size();
	std::erase_if(peers, [&](quint64 peer) {
		return std::find(serverPinned.begin(), serverPinned.end(), peer)
			!= serverPinned.end();
	});
	return peers.size() != was;
}

} // namespace Nagram::Chats
