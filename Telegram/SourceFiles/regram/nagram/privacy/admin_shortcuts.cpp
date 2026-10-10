#include "regram/nagram/privacy/admin_shortcuts.h"

#include "regram/nagram/privacy/options.h"
#include "boxes/peers/edit_participants_box.h"
#include "boxes/peers/edit_peer_info_box.h"
#include "boxes/peers/edit_peer_invite_links.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "history/admin_log/history_admin_log_section.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

namespace Nagram::Privacy {
namespace {

struct Rights {
	bool permissions = false;
	bool inviteLinks = false;
	bool admins = false;
	bool members = false;
	bool kicked = false;
	bool recentActions = false;

	[[nodiscard]] bool any() const {
		return permissions || inviteLinks || kicked || recentActions;
	}
};

Rights ComputeRights(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return {
			.permissions = chat->canEditPermissions(),
			.inviteLinks = chat->canHaveInviteLink(),
			.admins = chat->amIn(),
			.members = chat->amIn(),
		};
	}
	const auto channel = peer->asChannel();
	if (!channel || channel->isMonoforum()) {
		return {};
	}
	const auto admin = channel->hasAdminRights() || channel->amCreator();
	return {
		.permissions = channel->canEditPermissions(),
		.inviteLinks = channel->canHaveInviteLink(),
		.admins = channel->canViewAdmins(),
		.members = channel->canViewMembers(),
		.kicked = admin && channel->canViewBanned(),
		.recentActions = admin,
	};
}

} // namespace

void AddAdminShortcuts(
		const Ui::Menu::MenuCallback &addAction,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	if (!ForDevice().Get(kAdminShortcuts)) {
		return;
	}
	const auto rights = ComputeRights(peer);
	if (!rights.any()) {
		return;
	}
	const auto weak = base::make_weak(controller);
	const auto run = [=](Fn<void(not_null<Window::SessionController*>)> f) {
		return [=] {
			if (const auto strong = weak.get()) {
				f(strong);
			}
		};
	};
	using Role = ParticipantsBoxController::Role;
	const auto participants = [=](Role role) {
		return run([=](not_null<Window::SessionController*> strong) {
			ParticipantsBoxController::Start(strong, peer, role);
		});
	};
	addAction(Ui::Menu::MenuCallback::Args{
		.text = tr::lng_nagram_admin_shortcuts(tr::now),
		.handler = nullptr,
		.icon = &st::menuIconManage,
		.fillSubmenu = [&](not_null<Ui::PopupMenu*> menu) {
			if (rights.permissions) {
				menu->addAction(
					tr::lng_manage_peer_permissions(tr::now),
					run([=](not_null<Window::SessionController*> strong) {
						ShowEditChatPermissions(strong, peer);
					}),
					&st::menuIconPermissions);
			}
			if (rights.inviteLinks) {
				menu->addAction(
					tr::lng_manage_peer_invite_links(tr::now),
					run([=](not_null<Window::SessionController*> strong) {
						strong->show(Box(
							ManageInviteLinksBox,
							peer,
							peer->session().user(),
							0,
							0));
					}),
					&st::menuIconLinks);
			}
			if (rights.admins) {
				menu->addAction(
					tr::lng_manage_peer_administrators(tr::now),
					participants(Role::Admins),
					&st::menuIconAdmin);
			}
			if (rights.members) {
				menu->addAction(
					(peer->isBroadcast()
						? tr::lng_manage_peer_subscribers
						: tr::lng_manage_peer_members)(tr::now),
					participants(Role::Members),
					&st::menuIconGroups);
			}
			if (rights.kicked) {
				menu->addAction(
					tr::lng_manage_peer_removed_users(tr::now),
					participants(Role::Kicked),
					&st::menuIconRemovedUsers);
			}
			if (const auto channel = peer->asChannel()
				; channel && rights.recentActions) {
				menu->addAction(
					tr::lng_manage_peer_recent_actions(tr::now),
					run([=](not_null<Window::SessionController*> strong) {
						strong->showSection(
							std::make_shared<AdminLog::SectionMemento>(
								channel));
					}),
					&st::menuIconGroupLog);
			}
		},
	});
}

} // namespace Nagram::Privacy
