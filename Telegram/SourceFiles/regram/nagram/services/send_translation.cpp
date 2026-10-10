#include "regram/nagram/services/send_translation.h"

#include "regram/nagram/compose/confirm.h"
#include "regram/nagram/services/draft_translation.h"
#include "regram/nagram/services/send_translation_model.h"
#include "boxes/translate_box.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "ui/boxes/choose_language_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QPointer>

namespace Nagram::SendTranslation {
namespace {

constexpr auto kApprovedTimeout = crl::time(60'000);

struct Approved {
	QPointer<Ui::InputField> field;
	TextWithTags text;
	crl::time at = 0;
};

Approved LastApproved;

[[nodiscard]] LanguageId ChatLanguage(not_null<PeerData*> peer) {
	const auto name = Language(
		ForAccount(&peer->session()).Get(kChats),
		SerializePeerId(peer->id));
	return name.isEmpty() ? LanguageId() : LanguageId::FromName(name);
}

// The box resends the text it approved; that send must pass through.
[[nodiscard]] bool TakeApproved(not_null<Ui::InputField*> field) {
	const auto matches = (LastApproved.field == field.get())
		&& (crl::now() - LastApproved.at < kApprovedTimeout)
		&& (LastApproved.text == field->getTextWithTags());
	LastApproved = {};
	return matches;
}

void Save(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		LanguageId language) {
	auto &options = ForAccount(&peer->session());
	const auto updated = WithLanguage(
		options.Get(kChats),
		SerializePeerId(peer->id),
		language ? language.name() : QString());
	if (!updated || !options.Set(kChats, *updated)) {
		controller->showToast(tr::lng_nagram_send_translation_limit(
			tr::now,
			lt_amount,
			QString::number(kMaxChats)));
	}
}

void Choose(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	const auto current = ChatLanguage(peer);
	controller->show(Ui::ChooseTranslateToBox(
		current ? current : Ui::ChooseTranslateTo(LanguageId()),
		crl::guard(controller, [=](LanguageId chosen) {
			Save(controller, peer, chosen);
		})));
}

void ChatBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	box->setTitle(tr::lng_nagram_send_translation());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_send_translation_chat(
			lt_language,
			rpl::single(Ui::LanguageName(ChatLanguage(peer)))),
		st::boxLabel));
	box->addButton(tr::lng_nagram_send_translation_change(), [=] {
		box->closeBox();
		Choose(controller, peer);
	});
	box->addButton(tr::lng_nagram_send_translation_off(), [=] {
		Save(controller, peer, LanguageId());
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void AddPeerMenu(
		const Ui::Menu::MenuCallback &addAction,
		not_null<Window::SessionController*> controller,
		PeerData *peer) {
	if (!peer || peer->isSelf() || !ForDevice().Get(kEnabled)) {
		return;
	}
	const auto chat = not_null(peer);
	addAction(
		tr::lng_nagram_send_translation(tr::now),
		crl::guard(controller, [=] {
			if (ChatLanguage(chat)) {
				controller->show(Box(ChatBox, controller, chat));
			} else {
				Choose(controller, chat);
			}
		}),
		&st::menuIconTranslate);
}

rpl::producer<int> ChatsCountValue(not_null<Main::Session*> session) {
	return ForAccount(session).Value(
		kChats
	) | rpl::map([](const QByteArray &raw) {
		return int(Parse(raw).value_or(Chats()).size());
	});
}

void ClearChats(not_null<Main::Session*> session) {
	Expects(ForAccount(session).Set(kChats, QByteArray()));
}

} // namespace Nagram::SendTranslation

namespace Nagram::Compose {

bool TranslateBeforeSend(
		std::shared_ptr<Main::SessionShow> show,
		not_null<PeerData*> peer,
		Ui::InputField *field,
		Fn<void()> resend) {
	using namespace SendTranslation;
	if (!field || !ForDevice().Get(kEnabled)) {
		return false;
	}
	const auto language = ChatLanguage(peer);
	if (!language
		|| !Translatable(field->getLastText())
		|| TakeApproved(field)) {
		return false;
	}
	ShowTranslationBox(show, field, language, [=](
			not_null<Ui::GenericBox*> box,
			TextWithTags original,
			Fn<std::optional<TextWithEntities>()> result) {
		box->setTitle(tr::lng_nagram_send_translation());
		const auto send = [=](std::optional<TextWithEntities> translation) {
			if (field->getTextWithTags() != original) {
				box->showToast(tr::lng_nagram_draft_changed(tr::now));
				return;
			} else if (translation) {
				SetDraftTranslation(field, *translation);
			}
			LastApproved = {
				.field = field,
				.text = field->getTextWithTags(),
				.at = crl::now(),
			};
			const auto again = resend;
			box->closeBox();
			again();
		};
		box->addButton(
			tr::lng_nagram_send_translation_send(),
			crl::guard(field, [=] {
				const auto translation = result();
				if (translation && !translation->text.trimmed().isEmpty()) {
					send(translation);
				}
			}));
		box->addButton(
			tr::lng_nagram_send_translation_original(),
			crl::guard(field, [=] { send(std::nullopt); }));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	});
	return true;
}

} // namespace Nagram::Compose
