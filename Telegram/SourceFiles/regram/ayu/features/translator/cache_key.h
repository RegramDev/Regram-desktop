#pragma once

#include "base/assertion.h"
#include "ui/text/text_entity.h"

#include <QtCore/QByteArray>

namespace Ayu::Translator {

[[nodiscard]] QByteArray TranslationCacheKey(
	quint64 account,
	int provider,
	const TextWithEntities &text,
	const QString &from,
	const QString &to,
	quint64 peer = 0,
	qint64 message = 0);

} // namespace Ayu::Translator
