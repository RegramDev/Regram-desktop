#pragma once

namespace Dialogs::TopBarSuggestions {
enum class Priority : int;
} // namespace Dialogs::TopBarSuggestions

namespace Nagram::Chats {

[[nodiscard]] bool HideSponsoredMessages();
[[nodiscard]] bool HideProxySponsor();
[[nodiscard]] bool HidePhoneSuggestion();
[[nodiscard]] bool HideSuggestion(Dialogs::TopBarSuggestions::Priority priority);

} // namespace Nagram::Chats
