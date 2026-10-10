#include "regram/nagram/compose/chat_preferences.h"

#include "regram/nagram/core/owned_json.h"

#include <QtCore/QJsonArray>

namespace Nagram::Compose {

std::optional<ChatPreferenceSet> ParseChatPreferences(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return ChatPreferenceSet();
	} else if (raw.size() > 64 * 1024) {
		return std::nullopt;
	}
	const auto parsed = ParseOwnedJson(raw, { u"version"_q, u"user"_q, u"chats"_q });
	if (!parsed || !parsed->value(u"chats"_q).isArray()
		|| parsed->value(u"chats"_q).toArray().size() > 1000) {
		return std::nullopt;
	}
	auto result = ChatPreferenceSet{ .user = *DecimalId(parsed->value(u"user"_q)) };
	for (const auto entry : parsed->value(u"chats"_q).toArray()) {
		const auto object = entry.toObject();
		const auto peer = DecimalId(object.value(u"peer"_q));
		const auto formatting = object.value(u"formatting"_q);
		const auto spacing = object.value(u"spacing"_q);
		if (!entry.isObject() || object.size() != 3 || !peer
			|| !formatting.isDouble() || !spacing.isDouble()
			|| formatting.toDouble() != formatting.toInt()
			|| spacing.toDouble() != spacing.toInt()
			|| formatting.toInt() < -1 || formatting.toInt() > 5
			|| spacing.toInt() < -1 || spacing.toInt() > 1
			|| !result.chats.emplace(*peer, ChatPreferences{
				formatting.toInt(), spacing.toInt() }).second) {
			return std::nullopt;
		}
	}
	return result;
}

bool ValidChatPreferences(const QByteArray &raw) {
	return ParseChatPreferences(raw).has_value();
}

QByteArray EncodeChatPreferences(const ChatPreferenceSet &prefs) {
	if (prefs.chats.empty()) {
		return {};
	}
	auto object = NewOwnedJson(prefs.user);
	auto chats = QJsonArray();
	for (const auto &[peer, prefs] : prefs.chats) {
		chats.append(QJsonObject{
			{ u"peer"_q, QString::number(peer) },
			{ u"formatting"_q, prefs.formatting },
			{ u"spacing"_q, prefs.spacing },
		});
	}
	object.insert(u"chats"_q, chats);
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ChatPreferences PreferencesForChat(
		const QByteArray &raw,
		quint64 user,
		quint64 peer) {
	const auto prefs = ParseChatPreferences(raw);
	if (!prefs || prefs->user != user) {
		return {};
	}
	const auto found = prefs->chats.find(peer);
	return found == prefs->chats.end() ? ChatPreferences() : found->second;
}

} // namespace Nagram::Compose
