#include "regram/ayu/features/translator/cache_key.h"
#include "regram/nagram/services/translation_model.h"

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
