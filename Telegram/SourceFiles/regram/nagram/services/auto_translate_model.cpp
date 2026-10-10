#include "regram/nagram/services/auto_translate_model.h"

#include "base/basic_types.h"

namespace Nagram::AutoTranslate {
namespace {

constexpr auto kPeerFlag = quint64(0x80) << 48;
constexpr auto kPeerBareMask = quint64(0xFFFFFFFFFFFFULL);
constexpr auto kPeerTypes = 3;

} // namespace

bool ValidMode(const int &value) {
	return (value >= int(Mode::Inherit)) && (value <= int(Mode::Off));
}

bool ValidPeer(quint64 serialized) {
	return (serialized & kPeerFlag)
		&& !(serialized >> 56)
		&& (((serialized & ~kPeerFlag) >> 48) < kPeerTypes)
		&& (serialized & kPeerBareMask);
}

std::optional<ChatModes> ParseChats(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return ChatModes();
	} else if (raw.size() > kMaxChatsBytes) {
		return std::nullopt;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	const auto object = document.object();
	const auto chats = object.value(u"chats"_q);
	if (error.error != QJsonParseError::NoError
		|| !document.isObject()
		|| object.size() != 2
		|| object.value(u"version"_q) != QJsonValue(1)
		|| !chats.isObject()
		|| chats.toObject().size() > kMaxChats) {
		return std::nullopt;
	}
	auto result = ChatModes();
	const auto entries = chats.toObject();
	for (auto i = entries.begin(); i != entries.end(); ++i) {
		auto ok = false;
		const auto peer = i.key().toULongLong(&ok);
		const auto value = i.value().toString();
		if (!ok
			|| !ValidPeer(peer)
			|| QString::number(peer) != i.key()
			|| !i.value().isString()
			|| (value != u"on"_q && value != u"off"_q)) {
			return std::nullopt;
		}
		result.emplace(peer, (value == u"on"_q) ? Mode::On : Mode::Off);
	}
	return result;
}

QByteArray SerializeChats(const ChatModes &chats) {
	auto entries = QJsonObject();
	for (const auto &[peer, mode] : chats) {
		if (mode != Mode::Inherit) {
			entries.insert(
				QString::number(peer),
				(mode == Mode::On) ? u"on"_q : u"off"_q);
		}
	}
	return entries.isEmpty()
		? QByteArray()
		: QJsonDocument(QJsonObject{
			{ u"version"_q, 1 },
			{ u"chats"_q, entries },
		}).toJson(QJsonDocument::Compact);
}

bool ValidChats(const QByteArray &raw) {
	return ParseChats(raw).has_value();
}

Mode ChatMode(const QByteArray &raw, quint64 peer) {
	if (const auto chats = ParseChats(raw)) {
		if (const auto i = chats->find(peer); i != chats->end()) {
			return i->second;
		}
	}
	return Mode::Inherit;
}

std::optional<QByteArray> WithChatMode(
		const QByteArray &raw,
		quint64 peer,
		Mode mode) {
	auto chats = ParseChats(raw);
	if (!chats || !ValidPeer(peer)) {
		return std::nullopt;
	} else if (mode == Mode::Inherit) {
		chats->erase(peer);
	} else {
		(*chats)[peer] = mode;
	}
	auto result = SerializeChats(*chats);
	return (chats->size() > kMaxChats || result.size() > kMaxChatsBytes)
		? std::nullopt
		: std::make_optional(std::move(result));
}

Mode Resolve(Mode device, Mode account, Mode chat) {
	return (chat != Mode::Inherit)
		? chat
		: (account != Mode::Inherit)
		? account
		: device;
}

bool Tracking(bool enabled, bool upstream, State state) {
	return (state.mode != Mode::Off)
		&& enabled
		&& (upstream || (state.mode == Mode::On && state.service));
}

} // namespace Nagram::AutoTranslate
