#pragma once

#include "base/unique_qptr.h"

namespace style {
struct SettingsSlider;
namespace internal {
class Icon;
} // namespace internal
} // namespace style
namespace Data {
class ChatFilter;
} // namespace Data
namespace Ui {
class ChatsFiltersTabs;
class PopupMenu;
class VerticalLayout;
} // namespace Ui
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Chats {

[[nodiscard]] const style::SettingsSlider &FiltersTabsStyle(
	const style::SettingsSlider &fallback);
[[nodiscard]] bool GlobalSearchDisabled();
void WatchFolders(not_null<Window::SessionController*> controller);
void ResetFilterForFolder(not_null<Window::SessionController*> controller);
[[nodiscard]] bool RedirectFromAllChats(
	not_null<Window::SessionController*> controller);

[[nodiscard]] rpl::producer<> FolderListItemsChanges();
[[nodiscard]] std::vector<Data::ChatFilter> FolderTabs(
	std::vector<Data::ChatFilter> list,
	bool main);
[[nodiscard]] std::vector<const style::internal::Icon*> FolderTabIcons(
	const std::vector<Data::ChatFilter> &tabs,
	std::vector<const style::internal::Icon*> icons);
[[nodiscard]] int ShownFolderTab(
	not_null<Window::SessionController*> controller,
	const std::vector<Data::ChatFilter> &tabs,
	int fallback);
void FolderTabChosen(
	not_null<Window::SessionController*> controller,
	FilterId id);
[[nodiscard]] base::unique_qptr<Ui::PopupMenu> FolderTabMenu(
	not_null<QWidget*> parent,
	not_null<Window::SessionController*> controller,
	int offset);
void WatchArchiveTab(
	not_null<Window::SessionController*> controller,
	not_null<Ui::ChatsFiltersTabs*> slider,
	const std::vector<Data::ChatFilter> &tabs,
	Fn<void(int)> activate,
	rpl::lifetime &lifetime);
[[nodiscard]] bool FolderButtonActive(
	not_null<Window::SessionController*> controller);
void SetupFolderListButtons(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller);

} // namespace Nagram::Chats
