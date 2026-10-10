#include "regram/nagram/privacy/alias.h"

#include "data/data_peer.h"
#include "data/data_channel.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"

#include <map>
#include <memory>

namespace Nagram::Privacy {
namespace {

constexpr auto kMaximumAliases = 1000;
constexpr auto kMaximumAliasLength = 96;

struct State {
	base::weak_ptr<Main::Session> guard;
	PeerAliases aliases;
};

auto &States() {
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	return states;
}

State &ForSession(not_null<Main::Session*> session) {
	auto &states = States();
	if (const auto i = states.find(session)
		; i != states.end() && i->second->guard) {
		return *i->second;
	}
	// Names are resolved while the session is still being constructed, so
	// its lifetime can't own the state.
	std::erase_if(states, [](const auto &entry) {
		return !entry.second->guard;
	});
	const auto raw = ForAccount(session).Get(kAliases);
	auto state = std::make_unique<State>();
	state->guard = base::make_weak(session);
	state->aliases = ParseAliases(raw).value_or(PeerAliases());
	const auto inserted = states.emplace(session, std::move(state)).first;
	return *inserted->second;
}

} // namespace

const QString &Alias(not_null<const PeerData*> peer) {
	const auto &aliases = ForSession(&peer->session()).aliases;
	const auto i = aliases.find(peer->id);
	static const auto empty = QString();
	return (i != aliases.end()) ? i->second : empty;
}

const QString &DisplayName(not_null<const PeerData*> peer) {
	if (const auto to = peer->migrateTo()) {
		return DisplayName(to);
	} else if (const auto broadcast = peer->monoforumBroadcast()) {
		return DisplayName(broadcast);
	}
	const auto &alias = Alias(peer);
	return alias.isEmpty() ? peer->name() : alias;
}

QString SetAlias(
		not_null<PeerData*> peer,
		const QString &value,
		const QString &expected) {
	if (!ValidAlias(value)) {
		return tr::lng_nagram_alias_invalid(tr::now);
	}
	auto &state = ForSession(&peer->session());
	if (Alias(peer) != expected) {
		return tr::lng_nagram_alias_changed(tr::now);
	}
	auto aliases = state.aliases;
	if (value.isEmpty()) {
		aliases.remove(peer->id);
	} else {
		aliases[peer->id] = value;
	}
	if (aliases.size() > kMaximumAliases
		|| !ForAccount(&peer->session()).Set(kAliases, SerializeAliases(aliases))) {
		return tr::lng_nagram_alias_invalid(tr::now);
	}
	state.aliases = std::move(aliases);
	peer->localNameChanged();
	return {};
}

void ShowAlias(
		std::shared_ptr<Main::SessionShow> show,
		not_null<PeerData*> peer) {
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_peer_alias());
		const auto original = Alias(peer);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, rpl::single(peer->name()), st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
			tr::lng_nagram_peer_alias(), original));
		field->setMaxLength(kMaximumAliasLength);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_alias_about(), st::boxLabel));
		box->addButton(tr::lng_settings_save(), [=] {
			const auto error = SetAlias(peer, field->getLastText(), original);
			if (!error.isEmpty()) {
				box->showToast(error);
				field->showError();
				return;
			}
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram::Privacy
