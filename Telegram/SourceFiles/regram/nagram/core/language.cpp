#include "regram/nagram/core/language.h"

#include "lang/lang_file_parser.h"
#include "lang/lang_instance.h"
#include "lang/lang_tag.h"
#include "base/debug_log.h"

#include "lang_auto_counts.h"
#include "lang_auto.h"

#include <QtCore/QFile>
#include <QtCore/QLocale>

#include <map>
#include <optional>

namespace Nagram {
namespace {

struct Translation {
	QByteArray name;
	QString value;
};

using Translations = std::map<ushort, Translation>;

std::optional<QString> ParseValue(
		ushort key,
		const QByteArray &source) {
	auto result = QString();
	auto from = 0;
	for (auto i = 0; i != source.size(); ++i) {
		if (source[i] != '{') {
			continue;
		}
		const auto end = source.indexOf('}', i + 1);
		if (end < 0) {
			return {};
		}
		const auto tag = source.mid(i + 1, end - i - 1);
		const auto index = Lang::GetTagIndex(QLatin1String(tag));
		if (index == Lang::kTagsCount || !Lang::IsTagReplaced(key, index)) {
			return {};
		}
		result += QString::fromUtf8(source.constData() + from, i - from);
		auto replacement = QString(Lang::kTagReplacementSize,
			QChar(Lang::kTextCommand));
		replacement[1] = QChar(Lang::kTextCommandLangTag);
		replacement[2] = QChar(0x20 + index);
		result += replacement;
		from = end + 1;
		i = end;
	}
	result += QString::fromUtf8(source.constData() + from,
		source.size() - from);
	return result;
}

Translations Load(const QString &path) {
	auto result = Translations();
	const auto content = Lang::FileParser::ReadFile(path, path);
	const auto parser = Lang::FileParser(content,
		[&](QLatin1String name, const QByteArray &source) {
			const auto raw = QByteArray(name.data(), name.size());
			if (!raw.startsWith("lng_nagram_")
				&& !raw.startsWith("lng_regram_")) {
				return;
			}
			const auto key = Lang::GetKeyIndex(name);
			if (key == Lang::kKeysCount) {
				return;
			}
			if (const auto value = ParseValue(key, source)) {
				result.emplace(key, Translation{ raw, *value });
			}
		});
	if (!parser.errors().isEmpty()) {
		LOG(("Nagram language resource error: %1 (%2)"
			).arg(path, parser.errors()));
	}
	return result;
}

const Translations &Simplified() {
	static const auto empty = Translations();
	static auto simplified = std::optional<Translations>();
	const auto path = u":/langs/nagram/zh-hans.strings"_q;
	if (!simplified && QFile::exists(path)) {
		simplified = Load(path);
	}
	return simplified ? *simplified : empty;
}

const Translations &Traditional() {
	static const auto empty = Translations();
	static auto traditional = std::optional<Translations>();
	const auto path = u":/langs/nagram/zh-hant.strings"_q;
	if (!traditional && QFile::exists(path)) {
		traditional = Load(path);
	}
	return traditional ? *traditional : empty;
}

const Translations &Defaults(const QString &language) {
	static const auto empty = Translations();
	auto parts = language.toLower().replace(u'_', u'-').split(u'-');
	if (parts.size() > 1 && parts[1].size() == 4) {
		parts[1][0] = parts[1][0].toUpper();
	}
	const auto locale = QLocale(parts.join(u'-'));
	if (locale.language() != QLocale::Chinese) {
		return empty;
	}
	const auto isTraditional = (locale.script() == QLocale::TraditionalHanScript);
	return isTraditional ? Traditional() : Simplified();
}

struct LanguageCache {
	const Lang::Instance *instance = nullptr;
	QString id;
	QString baseId;
	const Translations *translations = nullptr;
	rpl::lifetime lifetime;
	bool valid = false;
};

const Translations &Selected(const Lang::Instance &instance) {
	static auto cache = LanguageCache();
	if (cache.instance != &instance) {
		cache.lifetime.destroy();
		cache.instance = &instance;
		instance.idChanges() | rpl::on_next([](const QString &) {
			cache.valid = false;
		}, cache.lifetime);
		cache.valid = false;
	}
	const auto id = instance.id();
	const auto baseId = instance.baseId();
	if (!cache.valid || cache.id != id || cache.baseId != baseId) {
		cache.id = id;
		cache.baseId = baseId;
		const auto &primary = Defaults(id);
		cache.translations = &(primary.empty() ? Defaults(baseId) : primary);
		cache.valid = true;
	}
	return *cache.translations;
}

} // namespace

QString LocalizedValue(const Lang::Instance &instance, ushort key) {
	const auto current = instance.getValue(key);
	if (!Simplified().contains(key)) {
		return current;
	}
	const auto &translations = Selected(instance);
	const auto found = translations.find(key);
	if (found == translations.end()) {
		return current;
	}
	const auto custom = instance.getNonDefaultValue(found->second.name);
	if (!custom.isEmpty()
		&& (instance.id() == Lang::CustomLanguageId()
			|| custom != Lang::GetOriginalValue(key))) {
		return current;
	}
	return found->second.value;
}

} // namespace Nagram
