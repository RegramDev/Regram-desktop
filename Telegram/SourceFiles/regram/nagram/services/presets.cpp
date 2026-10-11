#include "regram/nagram/services/presets.h"

namespace Nagram {
namespace {

using Kind = ServiceKind;

constexpr auto kChat = u"chat/completions";

// Sources of every address are listed in docs/nagram/p3-04-translation-llm.md.
constexpr ServicePreset kPresets[] = {
	{ "openai", u"OpenAI", Kind::Translation, u"openai",
		u"https://api.openai.com/v1/", kChat },
	{ "gemini", u"Gemini", Kind::Translation, u"openai",
		u"https://generativelanguage.googleapis.com/v1beta/openai/", kChat },
	{ "groq", u"Groq", Kind::Translation, u"openai",
		u"https://api.groq.com/openai/v1/", kChat },
	{ "deepseek", u"DeepSeek", Kind::Translation, u"openai",
		u"https://api.deepseek.com/v1/", kChat },
	{ "xai", u"xAI", Kind::Translation, u"openai",
		u"https://api.x.ai/v1/", kChat },
	{ "zhipu", u"Zhipu AI", Kind::Translation, u"openai",
		u"https://open.bigmodel.cn/api/paas/v4/", kChat },
	{ "mistral", u"Mistral", Kind::Translation, u"openai",
		u"https://api.mistral.ai/v1/", kChat },
	{ "openrouter", u"OpenRouter", Kind::Translation, u"openai",
		u"https://openrouter.ai/api/v1/", kChat },
	{ "qwen", u"Qwen", Kind::Translation, u"openai",
		u"https://dashscope.aliyuncs.com/compatible-mode/v1/", kChat },
	{ "moonshot", u"Moonshot", Kind::Translation, u"openai",
		u"https://api.moonshot.cn/v1/", kChat },
	{ "siliconflow", u"SiliconFlow", Kind::Translation, u"openai",
		u"https://api.siliconflow.cn/v1/", kChat },
	{ "anthropic", u"Anthropic", Kind::Translation, u"anthropic",
		u"https://api.anthropic.com/v1/", u"messages" },
	{ "deepl", u"DeepL", Kind::Translation, u"deepl",
		u"https://api.deepl.com/v2/", u"translate" },
	{ "google", u"Google Cloud Translation", Kind::Translation, u"google",
		u"https://translation.googleapis.com/", u"language/translate/v2" },
	{ "microsoft", u"Microsoft Translator", Kind::Translation, u"microsoft",
		u"https://api.cognitive.microsofttranslator.com/", u"translate" },
};

} // namespace

std::span<const ServicePreset> ServicePresets() {
	return kPresets;
}

ServiceDefinition ServiceFromPreset(
		const ServicePreset &preset,
		const QString &id,
		const QString &credentialRef) {
	return {
		.id = id,
		.name = QString::fromUtf16(preset.name),
		.kind = preset.kind,
		.protocol = QString::fromUtf16(preset.protocol),
		.baseUrl = QUrl(QString::fromUtf16(preset.baseUrl), QUrl::StrictMode),
		.endpoint = QString::fromUtf16(preset.endpoint),
		.credentialRef = credentialRef,
	};
}

} // namespace Nagram
