#include "regram/nagram/services/send_translation_model.h"

#include "regram/nagram/core/owned_json.h"

#include <algorithm>

namespace Nagram::SendTranslation {

bool ValidLanguage(const QString &language) {
	return !language.isEmpty()
		&& language.size() <= kMaxLanguageLength
		&& language[0].unicode() < 128
		&& language[0].isLetter()
		&& std::ranges::all_of(language, [](QChar ch) {
			return (ch.unicode() < 128)
				&& (ch.isLetterOrNumber() || ch == u'_' || ch == u'-');
		});
}

std::optional<Chats> Parse(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return Chats();
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	const auto object = document.object();
	const auto version = object.value(u"version"_q);
	const auto chats = object.value(u"chats"_q);
	if (error.error != QJsonParseError::NoError
		|| !document.isObject()
		|| object.size() != 2
		|| !version.isDouble()
		|| version.toDouble() != 1.
		|| !chats.isObject()
		|| chats.toObject().size() > kMaxChats) {
		return std::nullopt;
	}
	auto result = Chats();
	const auto list = chats.toObject();
	for (auto i = list.begin(); i != list.end(); ++i) {
		const auto peer = DecimalId(i.key());
		const auto language = i.value().toString();
		if (!peer || !i.value().isString() || !ValidLanguage(language)) {
			return std::nullopt;
		}
		result.emplace(*peer, language);
	}
	return result;
}

QByteArray Serialize(const Chats &chats) {
	if (chats.empty()) {
		return QByteArray();
	}
	auto list = QJsonObject();
	for (const auto &[peer, language] : chats) {
		list.insert(QString::number(peer), language);
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"chats"_q, list },
	}).toJson(QJsonDocument::Compact);
}

bool Valid(const QByteArray &raw) {
	return Parse(raw).has_value();
}

QString Language(const QByteArray &raw, uint64 peer) {
	const auto chats = Parse(raw).value_or(Chats());
	const auto i = chats.find(peer);
	return (i != chats.end()) ? i->second : QString();
}

std::optional<QByteArray> WithLanguage(
		const QByteArray &raw,
		uint64 peer,
		const QString &language) {
	auto chats = Parse(raw);
	if (!chats || !peer) {
		return std::nullopt;
	} else if (language.isEmpty()) {
		chats->erase(peer);
	} else if (!ValidLanguage(language)
		|| (!chats->contains(peer) && chats->size() >= kMaxChats)) {
		return std::nullopt;
	} else {
		(*chats)[peer] = language;
	}
	return Serialize(*chats);
}

bool Translatable(const QString &text) {
	const auto trimmed = text.trimmed();
	return !trimmed.startsWith(u'/')
		&& std::ranges::any_of(trimmed, [](QChar ch) {
			return ch.isLetter();
		});
}

} // namespace Nagram::SendTranslation
