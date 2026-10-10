#include "regram/nagram/chats/folders.h"

#include "regram/nagram/chats/options.h"
#include "boxes/choose_filter_box.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/ui_integration.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_session.h"
#include "data/data_unread_value.h"
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_main_list.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "menu/menu_mark_as_read.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/chat_filters_tabs_mode.h"
#include "ui/widgets/chat_filters_tabs_slider.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/side_bar_button.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_separate_id.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_nagram_compose.h"
#include "styles/style_window.h"

namespace Nagram::Chats {
namespace {

void ChooseFoldersBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<History*> history) {
	box->setTitle(tr::lng_nagram_join_folders_title());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_join_folders_about(),
		st::boxLabel));
	struct Row {
		FilterId id = 0;
		bool was = false;
		not_null<Ui::Checkbox*> checkbox;
	};
	auto rows = std::vector<Row>();
	for (const auto &filter : history->owner().chatsFilters().list()) {
		if (!filter.id()) {
			continue;
		}
		const auto was = filter.contains(history);
		rows.push_back({
			.id = filter.id(),
			.was = was,
			.checkbox = box->addRow(object_ptr<Ui::Checkbox>(
				box,
				filter.titleText().text,
				was)),
		});
	}
	box->addButton(tr::lng_settings_save(), [=] {
		const auto validator = ChooseFilterValidator(history);
		auto failed = false;
		for (const auto &row : rows) {
			const auto now = row.checkbox->checked();
			if (now == row.was) {
				continue;
			} else if (now && validator.canAdd(row.id)) {
				validator.add(row.id);
			} else if (!now && validator.canRemove(row.id)) {
				validator.remove(row.id);
			} else {
				failed = true;
			}
		}
		if (failed) {
			controller->showToast(tr::lng_nagram_join_folders_failed(tr::now));
		}
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

constexpr auto kArchiveTabId = FilterId(-2);
constexpr auto kSavedTabId = FilterId(-1);

// WHY: openFolder resets the filter to "All chats" before it marks the
// folder opened, so the redirect away from a hidden "All chats" has to
// be told that this reset is on purpose.
Window::SessionController *EnteringFolder = nullptr;

void WatchJoinedChats(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	session->changes().peerUpdates(
		Data::PeerUpdate::Flag::ChannelAmIn
	) | rpl::filter([=](const Data::PeerUpdate &update) {
		const auto channel = update.peer->asChannel();
		return channel
			&& channel->amIn()
			&& ForDevice().Get(kChooseFolderAfterJoin)
			&& (controller->activeChatCurrent().peer() == update.peer)
			&& session->data().chatsFilters().has();
	}) | rpl::on_next([=](const Data::PeerUpdate &update) {
		const auto history = session->data().history(update.peer);
		controller->show(Box(ChooseFoldersBox, controller, history));
	}, controller->lifetime());
}

void WatchFolderReturn(not_null<Window::SessionController*> controller) {
	const auto origin = controller->lifetime().make_state<FilterId>(0);
	controller->activeChatsFilter(
	) | rpl::combine_previous(
	) | rpl::on_next([=](FilterId was, FilterId now) {
		*origin = (EnteringFolder == controller && !now) ? was : FilterId();
	}, controller->lifetime());

	controller->openedFolder().changes(
	) | rpl::filter([=](Data::Folder *folder) {
		return !folder && !controller->activeChatsFilterCurrent();
	}) | rpl::on_next([=] {
		const auto filters = &controller->session().data().chatsFilters();
		const auto wanted = base::take(*origin);
		const auto exists = wanted
			&& ranges::contains(
				filters->list(),
				wanted,
				&Data::ChatFilter::id);
		const auto id = exists
			? wanted
			: filters->allChatsHidden()
			? filters->displayList().front().id()
			: FilterId();
		if (id) {
			controller->setActiveChatsFilter(id);
		}
	}, controller->lifetime());
}

} // namespace

const style::SettingsSlider &FiltersTabsStyle(
		const style::SettingsSlider &fallback) {
	return ForDevice().Get(kCompactFolderTabs)
		? st::nagramCompactFiltersTabs
		: fallback;
}

bool GlobalSearchDisabled() {
	return ForDevice().Get(kDisableGlobalSearch);
}

void WatchFolders(not_null<Window::SessionController*> controller) {
	WatchJoinedChats(controller);
	WatchFolderReturn(controller);
}

void ResetFilterForFolder(not_null<Window::SessionController*> controller) {
	const auto was = std::exchange(EnteringFolder, controller.get());
	const auto guard = gsl::finally([=] { EnteringFolder = was; });
	controller->setActiveChatsFilter(0);
}

bool RedirectFromAllChats(not_null<Window::SessionController*> controller) {
	return (EnteringFolder != controller)
		&& !controller->openedFolder().current()
		&& controller->session().data().chatsFilters().allChatsHidden();
}

namespace {

struct ArchiveUnread {
	int count = 0;
	bool muted = false;
};

[[nodiscard]] rpl::producer<ArchiveUnread> ArchiveUnreadValue(
		not_null<Main::Session*> session) {
	const auto list = session->data().folder(Data::Folder::kId)->chatsList();
	return rpl::combine(
		rpl::single(rpl::empty) | rpl::then(
			list->unreadStateChanges() | rpl::to_empty),
		Data::IncludeMutedCounterFoldersValue(),
		ForDevice().Value(kHideFolderUnreadCounters)
	) | rpl::map([=](rpl::empty_value, bool includeMuted, bool hidden) {
		const auto state = list->unreadState();
		const auto muted = state.chatsMuted + state.marksMuted;
		const auto count = (state.chats + state.marks)
			- (includeMuted ? 0 : muted);
		return ArchiveUnread{
			.count = hidden ? 0 : count,
			.muted = includeMuted && (count == muted),
		};
	});
}

[[nodiscard]] base::unique_qptr<Ui::PopupMenu> SavedFolderMenu(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller) {
	auto result = base::make_unique_q<Ui::PopupMenu>(
		parent,
		st::popupMenuWithIcons);
	const auto history = controller->session().data().history(
		controller->session().userPeerId());
	const auto addAction = Ui::Menu::CreateAddActionCallback(result.get());
	addAction(tr::lng_context_new_window(tr::now), crl::guard(controller, [=] {
		controller->showInNewWindow(Window::SeparateId(
			Window::SeparateType::Chat,
			history));
	}), &st::menuIconNewWindow);
	addAction(tr::lng_dlg_filter(tr::now), crl::guard(controller, [=] {
		controller->searchInChat(history);
	}), &st::menuIconSearch);
	addAction(tr::lng_nagram_hide_folder_entry(tr::now), [] {
		Expects(ForDevice().Set(kSavedInFolderList, false));
	}, &st::menuIconCancel);
	return result;
}

[[nodiscard]] base::unique_qptr<Ui::PopupMenu> ArchiveFolderMenu(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller) {
	auto result = base::make_unique_q<Ui::PopupMenu>(
		parent,
		st::popupMenuWithIcons);
	const auto session = &controller->session();
	const auto addAction = Ui::Menu::CreateAddActionCallback(result.get());
	addAction(tr::lng_context_new_window(tr::now), crl::guard(controller, [=] {
		controller->showInNewWindow(Window::SeparateId(
			Window::SeparateType::Archive,
			session));
	}), &st::menuIconNewWindow);
	if (const auto folder = session->data().folderLoaded(Data::Folder::kId)) {
		MarkAsReadMenu::AddChatListAction(
			controller,
			MarkAsReadMenu::ChatListKind::Archive,
			[=] { return folder->chatsList(); },
			addAction);
	}
	addAction(tr::lng_nagram_hide_folder_entry(tr::now), [] {
		Expects(ForDevice().Set(kArchiveInFolderList, false));
	}, &st::menuIconCancel);
	return result;
}

} // namespace

rpl::producer<> FolderListItemsChanges() {
	return rpl::merge(
		ForDevice().Value(kArchiveInFolderList) | rpl::skip(1) | rpl::to_empty,
		ForDevice().Value(kSavedInFolderList) | rpl::skip(1) | rpl::to_empty);
}

std::vector<Data::ChatFilter> FolderTabs(
		std::vector<Data::ChatFilter> list,
		bool main) {
	const auto add = [&](FilterId id, const QString &title) {
		list.push_back(Data::ChatFilter(
			id,
			{ TextWithEntities{ title } },
			QString(),
			std::nullopt,
			Data::ChatFilter::Flags(),
			{},
			{},
			{}));
	};
	if (main && ForDevice().Get(kArchiveInFolderList)) {
		add(kArchiveTabId, tr::lng_archived_name(tr::now));
	}
	if (main && ForDevice().Get(kSavedInFolderList)) {
		add(kSavedTabId, tr::lng_saved_messages(tr::now));
	}
	return list;
}

std::vector<const style::internal::Icon*> FolderTabIcons(
		const std::vector<Data::ChatFilter> &tabs,
		std::vector<const style::internal::Icon*> icons) {
	for (auto i = 0, count = int(tabs.size()); i != count; ++i) {
		if (tabs[i].id() == kArchiveTabId) {
			icons[i] = &st::nagramFoldersTabsArchive;
		} else if (tabs[i].id() == kSavedTabId) {
			icons[i] = &st::nagramFoldersTabsSaved;
		}
	}
	return icons;
}

int ShownFolderTab(
		not_null<Window::SessionController*> controller,
		const std::vector<Data::ChatFilter> &tabs,
		int fallback) {
	const auto find = [&](FilterId id) {
		const auto i = ranges::find(tabs, id, &Data::ChatFilter::id);
		return (i != end(tabs)) ? int(i - begin(tabs)) : -1;
	};
	const auto archive = controller->openedFolder().current()
		? find(kArchiveTabId)
		: -1;
	const auto shown = (archive >= 0)
		? archive
		: find(controller->activeChatsFilterCurrent());
	return (shown >= 0) ? shown : fallback;
}

void FolderTabChosen(
		not_null<Window::SessionController*> controller,
		FilterId id) {
	const auto session = &controller->session();
	if (id == kSavedTabId) {
		controller->showPeerHistory(session->userPeerId());
	} else if (const auto f = session->data().folderLoaded(Data::Folder::kId)) {
		controller->openFolder(f);
	}
}

base::unique_qptr<Ui::PopupMenu> FolderTabMenu(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		int offset) {
	const auto tabs = FolderTabs({}, true);
	if (offset < 0 || offset >= int(tabs.size())) {
		return nullptr;
	}
	return (tabs[offset].id() == kArchiveTabId)
		? ArchiveFolderMenu(parent, controller)
		: SavedFolderMenu(parent, controller);
}

void WatchArchiveTab(
		not_null<Window::SessionController*> controller,
		not_null<Ui::ChatsFiltersTabs*> slider,
		const std::vector<Data::ChatFilter> &tabs,
		Fn<void(int)> activate,
		rpl::lifetime &lifetime) {
	const auto i = ranges::find(tabs, kArchiveTabId, &Data::ChatFilter::id);
	if (i == end(tabs)) {
		return;
	}
	const auto index = int(i - begin(tabs));
	ArchiveUnreadValue(
		&controller->session()
	) | rpl::on_next([=](ArchiveUnread unread) {
		slider->setUnreadCount(index, unread.count, unread.muted);
		slider->fitWidthToSections();
	}, lifetime);
	controller->openedFolder().value(
	) | rpl::on_next([=] {
		if (const auto shown = ShownFolderTab(controller, tabs, -1)
			; shown >= 0) {
			activate(shown);
		}
	}, lifetime);
}

bool FolderButtonActive(not_null<Window::SessionController*> controller) {
	return !ForDevice().Get(kArchiveInFolderList)
		|| ((EnteringFolder != controller)
			&& !controller->openedFolder().current());
}

void SetupFolderListButtons(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	const auto holder = container->add(
		object_ptr<Ui::VerticalLayout>(container));
	const auto menu = holder->lifetime().make_state<
		base::unique_qptr<Ui::PopupMenu>>();
	rpl::combine(
		ForDevice().Value(kArchiveInFolderList),
		ForDevice().Value(kSavedInFolderList),
		Core::App().settings().chatFiltersTabsModeValue()
	) | rpl::on_next([=](
			bool archive,
			bool saved,
			Ui::ChatsFiltersTabsMode value) {
		using Mode = Ui::ChatsFiltersTabsMode;
		const auto mode = Ui::VerticalChatsFiltersTabsMode(value);
		const auto add = [&](FilterId id) {
			const auto isArchive = (id == kArchiveTabId);
			const auto button = holder->add(object_ptr<Ui::SideBarButton>(
				holder,
				TextWithEntities{ isArchive
					? tr::lng_archived_name(tr::now)
					: tr::lng_saved_messages(tr::now) },
				((mode == Mode::TextOnly)
					? st::windowFiltersButtonTextOnly
					: (mode == Mode::IconsOnly)
					? st::windowFiltersButtonIconsOnly
					: st::windowFiltersButton)));
			button->setIconOverride(
				isArchive ? &st::nagramFoldersArchive : &st::nagramFoldersSaved,
				(isArchive
					? &st::nagramFoldersArchiveActive
					: &st::nagramFoldersSavedActive));
			button->setShowIcon(mode != Mode::TextOnly);
			button->setShowText(mode != Mode::IconsOnly);
			button->setClickedCallback([=] {
				FolderTabChosen(controller, id);
			});
			button->events(
			) | rpl::filter([](not_null<QEvent*> e) {
				return (e->type() == QEvent::ContextMenu);
			}) | rpl::on_next([=](not_null<QEvent*> e) {
				*menu = isArchive
					? ArchiveFolderMenu(button, controller)
					: SavedFolderMenu(button, controller);
				(*menu)->popup(QCursor::pos());
				e->accept();
			}, button->lifetime());
			return button;
		};
		holder->clear();
		if (archive) {
			const auto button = add(kArchiveTabId);
			controller->openedFolder().value(
			) | rpl::on_next([=](Data::Folder *folder) {
				button->setActive(folder != nullptr);
			}, button->lifetime());
			ArchiveUnreadValue(
				&controller->session()
			) | rpl::on_next([=](ArchiveUnread unread) {
				button->setBadge(!unread.count
					? QString()
					: (unread.count > 999)
					? u"99+"_q
					: QString::number(unread.count), unread.muted);
			}, button->lifetime());
		}
		if (saved) {
			add(kSavedTabId);
		}
		holder->resizeToWidth(st::windowFiltersWidth);
	}, holder->lifetime());
}

} // namespace Nagram::Chats
