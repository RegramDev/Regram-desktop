#pragma once

class UserData;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class InputField;
} // namespace Ui

namespace Nagram::Compose {

[[nodiscard]] QString NicknameMentionTitle(not_null<UserData*> user);
void SetupNicknameMentions(
	not_null<Ui::InputField*> field,
	not_null<Main::Session*> session);

} // namespace Nagram::Compose
