#include "regram/nagram/media/local_faved.h"

#include "regram/nagram/media/local_faved_model.h"
#include "base/weak_ptr.h"
#include "core/version.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers.h"
#include "main/main_session.h"
#include "storage/serialize_document.h"

#include <QtCore/QDataStream>

#include <map>
#include <memory>

namespace Nagram::Media {
namespace {

struct State {
	base::weak_ptr<Main::Session> guard;
	std::vector<LocalFavedItem> items;
	std::vector<DocumentData*> documents;
	rpl::event_stream<> changes;
};

auto Enabled = false;
auto EnabledKnown = false;

[[nodiscard]] bool IsEnabled() {
	if (!EnabledKnown) {
		EnabledKnown = true;
		Enabled = ForDevice().Get(kUnlimitedFavedStickers);
	}
	return Enabled;
}

[[nodiscard]] QByteArray Serialize(not_null<DocumentData*> document) {
	auto result = QByteArray();
	auto stream = QDataStream(&result, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	Serialize::Document::writeToStream(stream, document);
	return result;
}

[[nodiscard]] DocumentData *Deserialize(
		not_null<Main::Session*> session,
		const LocalFavedItem &item) {
	auto stream = QDataStream(item.data);
	stream.setVersion(QDataStream::Qt_5_1);
	const auto document = Serialize::Document::readStickerFromStream(
		session,
		item.app,
		stream,
		Serialize::Document::StickerSetInfo(item.set, item.hash, QString()));
	return (stream.status() == QDataStream::Ok
		&& document
		&& document->sticker()
		&& document->id == item.id)
		? document
		: nullptr;
}

void Watch(not_null<Main::Session*> session);

[[nodiscard]] State &ForSession(not_null<Main::Session*> session) {
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	if (const auto i = states.find(session)
		; i != states.end() && i->second->guard) {
		return *i->second;
	}
	std::erase_if(states, [](const auto &entry) {
		return !entry.second->guard;
	});
	auto &state = *states.insert_or_assign(
		session,
		std::make_unique<State>()).first->second;
	state.guard = base::make_weak(session);
	auto skipped = 0;
	state.items = ParseLocalFaved(
		ForAccount(session).Get(kLocalFavedStickers),
		session->userId().bare,
		&skipped);
	state.documents.resize(state.items.size(), nullptr);
	for (auto i = 0; i != int(state.items.size()); ++i) {
		state.documents[i] = Deserialize(session, state.items[i]);
		skipped += state.documents[i] ? 0 : 1;
	}
	if (skipped) {
		LOG(("Nagram local favorites: %1 unreadable item(s) skipped."
			).arg(skipped));
	}
	crl::on_main(session, [=] { Watch(session); });
	return state;
}

void Save(not_null<Main::Session*> session) {
	auto &state = ForSession(session);
	Expects(ForAccount(session).Set(
		kLocalFavedStickers,
		SerializeLocalFaved(state.items, session->userId().bare)));
	state.changes.fire({});
}

[[nodiscard]] bool Remove(State &state, quint64 id) {
	for (auto i = 0; i != int(state.items.size()); ++i) {
		if (state.items[i].id == id) {
			state.items.erase(state.items.begin() + i);
			state.documents.erase(state.documents.begin() + i);
			return true;
		}
	}
	return false;
}

void MergeWithServer(not_null<Main::Session*> session) {
	const auto &sets = session->data().stickers().sets();
	const auto it = sets.find(Data::Stickers::FavedSetId);
	if (it == sets.cend()) {
		return;
	}
	auto &state = ForSession(session);
	auto changed = false;
	for (const auto &document : it->second->stickers) {
		changed |= Remove(state, document->id);
	}
	if (changed) {
		Save(session);
	}
}

void Watch(not_null<Main::Session*> session) {
	const auto stickers = &session->data().stickers();
	ForDevice().Value(
		kUnlimitedFavedStickers
	) | rpl::on_next([=](bool enabled) {
		const auto changed = EnabledKnown && (Enabled != enabled);
		EnabledKnown = true;
		Enabled = enabled;
		if (enabled) {
			MergeWithServer(session);
		}
		if (changed) {
			stickers->notifyUpdated(Data::StickersType::Stickers);
		}
	}, session->lifetime());

	stickers->updated(
		Data::StickersType::Stickers
	) | rpl::filter([] {
		return IsEnabled();
	}) | rpl::on_next([=] {
		MergeWithServer(session);
	}, session->lifetime());
}

} // namespace

bool KeepOverflowFaved(
		not_null<Main::Session*> session,
		not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	if (!IsEnabled() || !sticker || !sticker->set.id) {
		return false;
	}
	auto &state = ForSession(session);
	auto item = LocalFavedItem{
		.id = document->id,
		.set = sticker->set.id,
		.hash = sticker->set.accessHash,
		.app = AppVersion,
		.data = Serialize(document),
	};
	[[maybe_unused]] const auto was = Remove(state, document->id);
	if (!AddFavedItem(state.items, std::move(item))) {
		return false;
	}
	state.documents.insert(state.documents.begin(), document);
	Save(session);
	return true;
}

bool LocalFaved(not_null<const DocumentData*> document) {
	if (!IsEnabled()) {
		return false;
	}
	const auto &documents = ForSession(&document->session()).documents;
	return ranges::contains(documents, document.get());
}

void RemoveLocalFaved(not_null<DocumentData*> document) {
	const auto session = &document->session();
	if (IsEnabled() && Remove(ForSession(session), document->id)) {
		Save(session);
	}
}

Data::StickersPack WithLocalFaved(
		not_null<Main::Session*> session,
		Data::StickersPack pack) {
	if (!IsEnabled()) {
		return pack;
	}
	for (const auto &document : ForSession(session).documents) {
		if (document && !pack.contains(document)) {
			pack.push_back(document);
		}
	}
	return pack;
}

rpl::producer<int> LocalFavedCountValue(not_null<Main::Session*> session) {
	return rpl::single(rpl::empty) | rpl::then(
		ForSession(session).changes.events()
	) | rpl::map([=] {
		return int(ForSession(session).items.size());
	}) | rpl::distinct_until_changed();
}

void ClearLocalFaved(not_null<Main::Session*> session) {
	auto &state = ForSession(session);
	if (state.items.empty()) {
		return;
	}
	state.items.clear();
	state.documents.clear();
	Save(session);
	session->data().stickers().notifyUpdated(Data::StickersType::Stickers);
}

} // namespace Nagram::Media
