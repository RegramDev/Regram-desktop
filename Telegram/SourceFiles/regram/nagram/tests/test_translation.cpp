#include "regram/ayu/features/translator/cache_key.h"
#include "regram/nagram/services/translation_model.h"
#include "regram/nagram/compose/default_format.h"
#include "regram/nagram/compose/chat_preferences.h"

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestTranslation() {
	using namespace Nagram::Compose;
	const auto source = TextWithEntities{
		u"plain code quote emoji rest"_q,
		{
			EntityInText(EntityType::Code, 6, 4),
			EntityInText(EntityType::Blockquote, 11, 5),
			EntityInText(EntityType::CustomEmoji, 17, 5, u"42"_q),
		},
	};
	const auto formatted = ApplyDefaultFormat(source, DefaultFormat::Bold);
	Require(formatted.text == source.text && formatted.entities.size() == 5,
		"default formatting changes code, quote or custom emoji");
	const auto second = ApplyDefaultFormat(formatted, DefaultFormat::Bold);
	Require(second == formatted, "default formatting applies twice on repeated send preparation");
	const auto disabled = ApplyDefaultFormat(source, DefaultFormat::None);
	Require(disabled == source, "disabled default formatting alters entities");
	const auto manual = TextWithEntities{ u"manual text"_q, { EntityInText(EntityType::Italic, 0, 6) } };
	const auto mixed = ApplyDefaultFormat(manual, DefaultFormat::Underline);
	Require(mixed.entities.front() == manual.entities.front()
		&& mixed.entities.back().offset() == 6, "default formatting replaces manual formatting");
	const auto prefs = ChatPreferenceSet{
		.user = 42,
		.chats = { { 9007199254740993ULL, { 3, 1 } } },
	};
	const auto raw = EncodeChatPreferences(prefs);
	Require(PreferencesForChat(raw, 42, 9007199254740993ULL) == ChatPreferences{ 3, 1 },
		"per-chat preferences lose exact 64-bit peer IDs");
	Require(PreferencesForChat(raw, 43, 9007199254740993ULL) == ChatPreferences{},
		"per-chat preferences cross into another account");
	Require(PreferencesForChat(raw, 42, 1) == ChatPreferences{},
		"unconfigured chat does not inherit defaults");
	Require(!ValidChatPreferences(R"({"version":1,"user":"42","chats":[{"peer":"1","formatting":1.5,"spacing":0}]})"),
		"per-chat preferences accept non-integer formats");
	using namespace Nagram;
	using Ayu::Translator::TranslationCacheKey;
	auto original = TextWithEntities{ u"Do not pay"_q };
	const auto key = TranslationCacheKey(1, 1, original, u"auto"_q, u"zh"_q, 7, 42);
	Require(key == TranslationCacheKey(1, 1, original, u"auto"_q, u"zh"_q, 7, 42),
		"unchanged translation request misses its cache");
	original.text = u"Pay now"_q;
	Require(key != TranslationCacheKey(1, 1, original, u"auto"_q, u"zh"_q, 7, 42),
		"edited message reuses its old translation");
	Require(key != TranslationCacheKey(2, 1, { u"Do not pay"_q }, u"auto"_q, u"zh"_q, 7, 42),
		"translation cache is shared across accounts");
	Require(key != TranslationCacheKey(1, 2, { u"Do not pay"_q }, u"auto"_q, u"zh"_q, 7, 42),
		"translation cache is shared across providers");
	auto draft = TextWithEntities{
		u"Hello link code"_q,
		{
			EntityInText(EntityType::Bold, 0, 5),
			EntityInText(EntityType::CustomUrl, 6, 4, u"https://example.com/first"_q),
			EntityInText(EntityType::Code, 11, 4),
		},
	};
	const auto first = TranslationCacheKey(1, 1, draft, u"auto"_q, u"zh"_q);
	draft.entities[1] = EntityInText(
		EntityType::CustomUrl,
		6,
		4,
		u"https://example.com/second"_q);
	Require(first != TranslationCacheKey(1, 1, draft, u"auto"_q, u"zh"_q),
		"changing a hidden link reuses the old draft translation");
	const auto plan = PlanTranslation(draft);
	Require(plan && plan->texts == QStringList{ u"Hello"_q },
		"protected links or code are sent for translation");
	const auto translated = ApplyTranslation(*plan, { u"你好"_q });
	Require(translated && translated->text == u"你好 link code"_q
		&& translated->entities == EntitiesInText{
			EntityInText(EntityType::Bold, 0, 2),
			EntityInText(EntityType::CustomUrl, 3, 4, u"https://example.com/second"_q),
			EntityInText(EntityType::Code, 8, 4),
		}, "translated draft loses formatting or hidden link targets");
	Require(!ApplyTranslation(*plan, {})
		&& !ApplyTranslation(*plan, { QString() }),
		"partial or empty translation erases the draft");
	const auto protectedOnly = PlanTranslation({
		u"code"_q,
		{ EntityInText(EntityType::Code, 0, 4) },
	});
	Require(protectedOnly && protectedOnly->texts.empty()
		&& ApplyTranslation(*protectedOnly, {})->text == u"code"_q,
		"protected-only text requires a service request");
	Require(!PlanTranslation({
		u"\U0001F600"_q,
		{ EntityInText(EntityType::Bold, 1, 1) },
	}), "translation accepts an entity splitting a surrogate pair");
	std::cout << "PASS: Translation cache identity and structured output" << std::endl;
}
