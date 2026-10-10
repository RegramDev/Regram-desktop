#include "regram/ayu/features/translator/cache_key.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>

namespace Ayu::Translator {

QByteArray TranslationCacheKey(
		quint64 account,
		int provider,
		const TextWithEntities &text,
		const QString &from,
		const QString &to,
		quint64 peer,
		qint64 message) {
	auto bytes = QByteArray();
	auto stream = QDataStream(&bytes, QIODevice::WriteOnly);
	stream << account << qint32(provider) << from << to << peer << message;
	stream << text.text << qint32(text.entities.size());
	for (const auto &entity : text.entities) {
		stream << qint32(entity.type()) << qint32(entity.offset());
		stream << qint32(entity.length()) << entity.data();
	}
	return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

} // namespace Ayu::Translator
