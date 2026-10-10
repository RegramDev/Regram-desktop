#include "regram/nagram/services/auto_translate.h"

#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/nagram/services/chat_translation.h"
#include "regram/nagram/services/model.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_media_player.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram::AutoTranslate {
namespace {

[[nodiscard]] Mode ToMode(int value) {
	return ValidMode(value) ? Mode(value) : Mode::Inherit;
}

[[nodiscard]] quint64 Key(not_null<const PeerData*> peer) {
	return SerializePeerId(peer->id);
}

[[nodiscard]] bool SetChatMode(
		not_null<Main::Session*> session,
		quint64 peer,
		Mode mode) {
	const auto updated = WithChatMode(
		ForAccount(session).Get(kChats),
		peer,
		mode);
	return updated && ForAccount(session).Set(kChats, *updated);
}

void ModeBox(
		not_null<Ui::GenericBox*> box,
		rpl::producer<QString> title,
		const QString &inherit,
		Mode current,
		Fn<bool(Mode)> save) {
	box->setTitle(std::move(title));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(int(current));
	for (const auto &mode : { Mode::Inherit, Mode::On, Mode::Off }) {
		box->addRow(
			object_ptr<Ui::Radiobutton>(
				box,
				group,
				int(mode),
				ModeLabel(mode, inherit),
				st::settingsSendType),
			st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (!save(Mode(value))) {
			box->showToast(tr::lng_nagram_auto_translate_limit(
				tr::now,
				lt_amount,
				QString::number(kMaxChats)));
			return;
		}
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ChatModeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		quint64 peer,
		QString name) {
	ModeBox(
		box,
		rpl::single(std::move(name)),
		tr::lng_nagram_auto_translate_chat_inherit(tr::now),
		ChatMode(ForAccount(session).Get(kChats), peer),
		[=](Mode mode) { return SetChatMode(session, peer, mode); });
}

class ChooseChatController final : public ChatsListBoxController {
public:
	ChooseChatController(
		not_null<Main::Session*> session,
		Fn<void(not_null<PeerData*>)> chosen)
	: ChatsListBoxController(session)
	, _session(session)
	, _chosen(std::move(chosen)) {
	}

	Main::Session &session() const override {
		return *_session;
	}
	void rowClicked(not_null<PeerListRow*> row) override {
		_chosen(row->peer());
	}

protected:
	void prepareViewHook() override {
		delegate()->peerListSetTitle(tr::lng_nagram_auto_translate_chats_add());
	}
	std::unique_ptr<Row> createRow(not_null<History*> history) override {
		return history->peer->isSelf()
			? nullptr
			: std::make_unique<Row>(history);
	}

private:
	const not_null<Main::Session*> _session;
	const Fn<void(not_null<PeerData*>)> _chosen;

};

} // namespace

Mode For(not_null<History*> history) {
	const auto session = &history->session();
	return Resolve(
		ToMode(ForDevice().Get(kDeviceMode)),
		ToMode(ForAccount(session).Get(kAccountMode)),
		ChatMode(ForAccount(session).Get(kChats), Key(history->peer)));
}

bool Enabled(not_null<History*> history) {
	using Flag = PeerData::TranslationFlag;
	return (history->peer->translationFlag() != Flag::Disabled)
		&& (For(history) == Mode::On);
}

rpl::producer<State> StateValue(not_null<History*> history) {
	const auto session = &history->session();
	return rpl::combine(
		ForDevice().Value(kDeviceMode),
		ForAccount(session).Value(kAccountMode),
		ForAccount(session).Value(kChats),
		ForDevice().Value(kChatTranslationUseService),
		ForDevice().Value(kServicesConfig)
	) | rpl::map([=] {
		return State{ For(history), ChatTranslationServiceActive() };
	}) | rpl::distinct_until_changed();
}

void AddPeerMenu(
		const Ui::Menu::MenuCallback &addAction,
		not_null<Window::SessionController*> controller,
		PeerData *peer) {
	if (!peer || peer->isSelf()) {
		return;
	}
	const auto session = &controller->session();
	const auto key = Key(peer);
	const auto current = ChatMode(ForAccount(session).Get(kChats), key);
	if (current == Mode::Inherit
		&& !ForDevice().Get(kDeviceMode)
		&& !ForAccount(session).Get(kAccountMode)) {
		return;
	}
	addAction({
		.text = tr::lng_nagram_auto_translate_menu(tr::now),
		.handler = nullptr,
		.icon = &st::menuIconTranslate,
		.fillSubmenu = [=](not_null<Ui::PopupMenu*> menu) {
			const auto inherit = tr::lng_nagram_auto_translate_chat_inherit(
				tr::now);
			for (const auto &mode : { Mode::Inherit, Mode::On, Mode::Off }) {
				menu->addAction(
					ModeLabel(mode, inherit),
					crl::guard(controller, [=] {
						if (!SetChatMode(session, key, mode)) {
							controller->showToast(
								tr::lng_nagram_auto_translate_limit(
									tr::now,
									lt_amount,
									QString::number(kMaxChats)));
						}
					}),
					(mode == current) ? &st::mediaPlayerMenuCheck : nullptr);
			}
		},
	});
}

QString ModeLabel(Mode mode, const QString &inherit) {
	switch (mode) {
	case Mode::Inherit: return inherit;
	case Mode::On: return tr::lng_nagram_auto_translate_on(tr::now);
	case Mode::Off: return tr::lng_nagram_auto_translate_off(tr::now);
	}
	Unexpected("Invalid Nagram auto-translate mode.");
}

rpl::producer<QString> AboutValue(not_null<Main::Session*> session) {
	return rpl::combine(
		tr::lng_nagram_auto_translate_about(),
		tr::lng_nagram_auto_translate_unavailable(),
		Data::AmPremiumValue(session),
		ForDevice().Value(kChatTranslationUseService),
		ForDevice().Value(kServicesConfig)
	) | rpl::map([](
			const QString &about,
			const QString &unavailable,
			bool premium,
			bool,
			const QByteArray &) {
		return (premium || ChatTranslationServiceActive())
			? about
			: (about + u"\n\n"_q + unavailable);
	});
}

void DeviceModeBox(not_null<Ui::GenericBox*> box) {
	ModeBox(
		box,
		tr::lng_nagram_auto_translate(),
		tr::lng_nagram_auto_translate_inherit(tr::now),
		ToMode(ForDevice().Get(kDeviceMode)),
		[](Mode mode) { return ForDevice().Set(kDeviceMode, int(mode)); });
}

void AccountModeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	ModeBox(
		box,
		tr::lng_nagram_auto_translate_account(),
		tr::lng_nagram_auto_translate_account_inherit(tr::now),
		ToMode(ForAccount(session).Get(kAccountMode)),
		[=](Mode mode) {
			return ForAccount(session).Set(kAccountMode, int(mode));
		});
}

void ChatsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_auto_translate_chats());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_auto_translate_chats_about(),
		st::boxLabel));
	const auto rows = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		style::margins());
	const auto inherit = tr::lng_nagram_auto_translate_chat_inherit(tr::now);
	ForAccount(session).Value(
		kChats
	) | rpl::on_next([=](const QByteArray &raw) {
		rows->clear();
		const auto note = [&](rpl::producer<QString> text) {
			rows->add(
				object_ptr<Ui::FlatLabel>(rows, std::move(text), st::boxLabel),
				st::boxRowPadding);
		};
		if (ForAccount(session).invalidKeys().contains(kChats.key)) {
			note(tr::lng_nagram_auto_translate_invalid());
		}
		const auto chats = ParseChats(raw).value_or(ChatModes());
		if (chats.empty()) {
			note(tr::lng_nagram_auto_translate_chats_empty());
		}
		for (const auto &[key, mode] : chats) {
			const auto peer = session->data().peerLoaded(
				DeserializePeerId(key));
			const auto name = peer ? peer->name() : QString::number(key);
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows,
				tr::lng_nagram_auto_translate_chat_row(
					lt_name,
					rpl::single(name),
					lt_state,
					rpl::single(ModeLabel(mode, inherit))),
				st::settingsButtonNoIcon));
			row->setClickedCallback([=] {
				box->uiShow()->showBox(Box(ChatModeBox, session, key, name));
			});
		}
		rows->resizeToWidth(box->width());
	}, box->lifetime());
	box->addButton(tr::lng_nagram_auto_translate_chats_add(), [=] {
		const auto chooser = std::make_shared<base::weak_qptr<PeerListBox>>();
		auto chosen = crl::guard(box, [=](not_null<PeerData*> peer) {
			if (const auto strong = chooser->get()) {
				strong->closeBox();
			}
			box->uiShow()->showBox(
				Box(ChatModeBox, session, Key(peer), peer->name()));
		});
		*chooser = box->uiShow()->show(Box<PeerListBox>(
			std::make_unique<ChooseChatController>(session, std::move(chosen)),
			[](not_null<PeerListBox*> list) {
				list->addButton(tr::lng_cancel(), [=] { list->closeBox(); });
			}));
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Nagram::AutoTranslate
