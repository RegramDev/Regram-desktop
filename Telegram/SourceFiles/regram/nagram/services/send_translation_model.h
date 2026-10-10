#pragma once

#include "base/basic_types.h"
#include "regram/nagram/core/options.h"

#include <map>
#include <optional>

namespace Nagram::SendTranslation {

inline constexpr auto kMaxChats = 500;
inline constexpr auto kMaxLanguageLength = 16;

using Chats = std::map<uint64, QString>;

[[nodiscard]] bool ValidLanguage(const QString &language);
[[nodiscard]] std::optional<Chats> Parse(const QByteArray &raw);
[[nodiscard]] QByteArray Serialize(const Chats &chats);
[[nodiscard]] bool Valid(const QByteArray &raw);
[[nodiscard]] QString Language(const QByteArray &raw, uint64 peer);
[[nodiscard]] std::optional<QByteArray> WithLanguage(
	const QByteArray &raw,
	uint64 peer,
	const QString &language);
[[nodiscard]] bool Translatable(const QString &text);

inline constexpr auto kEnabled = Option<bool>{
	"nagram.sendTranslation", Scope::Device, false,
	Category::Services, "lng_nagram_send_translation" };
inline const auto kChats = Option<QByteArray>{
	"nagram.sendTranslationChats", Scope::Account, QByteArray(),
	Category::Services, "lng_nagram_send_translation_chats", 0,
	Valid };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kEnabled));
	Expects(registry.Add(kChats));
}

} // namespace Nagram::SendTranslation
