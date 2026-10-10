#pragma once

#include "base/assertion.h"

#include <QtCore/QByteArray>

#include <map>
#include <optional>

namespace Nagram::Compose {

struct ChatPreferences {
	int formatting = -1;
	int spacing = -1;
	friend bool operator==(const ChatPreferences &, const ChatPreferences &) = default;
};

struct ChatPreferenceSet {
	quint64 user = 0;
	std::map<quint64, ChatPreferences> chats;
};

[[nodiscard]] std::optional<ChatPreferenceSet> ParseChatPreferences(const QByteArray &raw);
[[nodiscard]] bool ValidChatPreferences(const QByteArray &raw);
[[nodiscard]] QByteArray EncodeChatPreferences(const ChatPreferenceSet &prefs);
[[nodiscard]] ChatPreferences PreferencesForChat(
	const QByteArray &raw,
	quint64 user,
	quint64 peer);

} // namespace Nagram::Compose
