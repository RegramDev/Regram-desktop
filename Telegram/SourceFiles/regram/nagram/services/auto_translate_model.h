#pragma once

#include "regram/nagram/core/options.h"

#include <map>
#include <optional>

namespace Nagram::AutoTranslate {

enum class Mode {
	Inherit,
	On,
	Off,
};

struct State {
	Mode mode = Mode::Inherit;
	bool service = false;

	friend inline bool operator==(State, State) = default;
};

using ChatModes = std::map<quint64, Mode>;

inline constexpr auto kMaxChats = 2000;
inline constexpr auto kMaxChatsBytes = 512 * 1024;

[[nodiscard]] bool ValidMode(const int &value);
[[nodiscard]] bool ValidPeer(quint64 serialized);
[[nodiscard]] std::optional<ChatModes> ParseChats(const QByteArray &raw);
[[nodiscard]] QByteArray SerializeChats(const ChatModes &chats);
[[nodiscard]] bool ValidChats(const QByteArray &raw);
[[nodiscard]] Mode ChatMode(const QByteArray &raw, quint64 peer);
[[nodiscard]] std::optional<QByteArray> WithChatMode(
	const QByteArray &raw,
	quint64 peer,
	Mode mode);

[[nodiscard]] Mode Resolve(Mode device, Mode account, Mode chat);
[[nodiscard]] bool Tracking(bool enabled, bool upstream, State state);

inline constexpr auto kDeviceMode = Option<int>{
	"nagram.autoTranslate", Scope::Device, 0,
	Category::Services, "lng_nagram_auto_translate", 0, ValidMode };
inline constexpr auto kAccountMode = Option<int>{
	"nagram.autoTranslateAccount", Scope::Account, 0,
	Category::Services, "lng_nagram_auto_translate_account", 0, ValidMode };
inline const auto kChats = Option<QByteArray>{
	"nagram.autoTranslateChats", Scope::Account, QByteArray(),
	Category::Services, "lng_nagram_auto_translate_chats", 0, ValidChats };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kDeviceMode));
	Expects(registry.Add(kAccountMode));
	Expects(registry.Add(kChats));
}

} // namespace Nagram::AutoTranslate
