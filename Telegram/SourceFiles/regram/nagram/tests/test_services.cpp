#include "regram/nagram/services/context_model.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/services/presets.h"
#include "regram/nagram/services/summary_model.h"
#include "regram/nagram/services/transcription_queue.h"
#include "base/basic_types.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

QByteArray OpenAiReply(const QByteArray &content, const QByteArray &reason) {
	return QJsonDocument(QJsonObject{ { u"choices"_q, QJsonArray{ QJsonObject{
		{ u"finish_reason"_q, reason.isEmpty()
			? QJsonValue()
			: QJsonValue(QString::fromUtf8(reason)) },
		{ u"message"_q, QJsonObject{
			{ u"role"_q, u"assistant"_q },
			{ u"content"_q, QString::fromUtf8(content) },
		} },
	} } } }).toJson();
}

void TestServicesV2(const Nagram::ServiceDefinition &service) {
	using namespace Nagram;
	auto oldInstance = SerializeService(service);
	oldInstance.remove(u"summaryPrompt"_q);
	const auto old = QJsonObject{
		{ u"version"_q, 1 },
		{ u"translation"_q, service.id },
		{ u"transcription"_q, QString() },
		{ u"instances"_q, QJsonArray{ oldInstance } },
	};
	Require(ValidServices(old)
		&& ValidServicesBytes(QJsonDocument(old).toJson()),
		"version 1 services rejected");
	const auto upgraded = UpgradeServices(old);
	Require(upgraded.value(u"version"_q) == QJsonValue(2)
		&& upgraded.value(u"summary"_q) == QString()
		&& upgraded.value(u"translation"_q) == service.id
		&& upgraded.value(u"instances"_q).toArray()[0]
			== QJsonValue(SerializeService(service)),
		"version 1 upgrade");
	Require(UpgradeServices(upgraded) == upgraded, "upgrade not idempotent");
	Require(UpgradeServices(ServicesDefaults()) == ServicesDefaults(),
		"defaults changed by upgrade");
	auto mixed = old;
	mixed.insert(u"summary"_q, QString());
	Require(!ValidServices(mixed), "version 1 with summary accepted");
	mixed = upgraded;
	mixed.insert(u"instances"_q, QJsonArray{ oldInstance });
	Require(!ValidServices(mixed), "version 2 without summaryPrompt accepted");
	mixed = upgraded;
	mixed.insert(u"version"_q, 3);
	Require(!ValidServices(mixed), "unknown version accepted");

	auto prompted = service;
	prompted.summaryPrompt = u"Summarize briefly.\nUse bullet points."_q;
	Require(ParseService(SerializeService(prompted)).has_value(),
		"summary prompt rejected");
	Require(CredentialAccount(prompted) == CredentialAccount(service),
		"summary prompt changed the credential binding");
	auto deepl = service;
	deepl.id = u"00000000-0000-0000-0000-000000000003"_q;
	deepl.protocol = u"deepl"_q;
	deepl.model = QString();
	Require(ParseService(SerializeService(deepl)).has_value(), "DeepL");
	auto deeplPrompted = deepl;
	deeplPrompted.summaryPrompt = u"x"_q;
	Require(!ParseService(SerializeService(deeplPrompted)),
		"summary prompt accepted for a non-LLM protocol");
	auto audio = service;
	audio.id = u"00000000-0000-0000-0000-000000000004"_q;
	audio.kind = ServiceKind::Transcription;
	Require(ParseService(SerializeService(audio)).has_value(), "audio");
	auto audioPrompted = audio;
	audioPrompted.summaryPrompt = u"x"_q;
	Require(!ParseService(SerializeService(audioPrompted)),
		"summary prompt accepted for transcription");

	auto config = ServicesDefaults();
	config.insert(u"instances"_q, QJsonArray{
		SerializeService(service),
		SerializeService(deepl),
		SerializeService(audio),
	});
	config.insert(u"summary"_q, service.id);
	Require(ValidServices(config), "LLM summary selection rejected");
	config.insert(u"summary"_q, deepl.id);
	Require(!ValidServices(config), "non-LLM summary selection accepted");
	config.insert(u"summary"_q, audio.id);
	Require(!ValidServices(config), "transcription summary accepted");
	config.insert(u"summary"_q, u"00000000-0000-0000-0000-000000000009"_q);
	Require(!ValidServices(config), "missing summary instance accepted");
	config.insert(u"summary"_q, u"system"_q);
	Require(!ValidServices(config), "system summary accepted");
	config.insert(u"summary"_q, 1);
	Require(!ValidServices(config), "non-string summary accepted");
	std::cout << "PASS: Nagram services config version 2" << std::endl;
}

void TestLlmProtocols(const Nagram::ServiceDefinition &service) {
	using namespace Nagram;
	auto anthropic = service;
	anthropic.protocol = u"anthropic"_q;
	anthropic.endpoint = u"messages"_q;
	anthropic.systemPrompt = u"Be exact."_q;
	anthropic.temperature = 1.;
	Require(ParseService(SerializeService(anthropic)).has_value(),
		"Anthropic service rejected");
	Require(LlmProtocol(u"anthropic"_q) && LlmProtocol(u"openai"_q)
		&& !LlmProtocol(u"deepl"_q), "LLM protocol list");
	auto invalid = anthropic;
	invalid.temperature = 1.5;
	Require(!ParseService(SerializeService(invalid)),
		"Anthropic temperature above 1 accepted");
	invalid = anthropic;
	invalid.model = QString();
	Require(!ParseService(SerializeService(invalid)),
		"Anthropic without a model accepted");
	invalid = anthropic;
	invalid.kind = ServiceKind::Transcription;
	invalid.systemPrompt = QString();
	Require(!ParseService(SerializeService(invalid)),
		"Anthropic transcription accepted");
	const auto call = BuildTranslationCall(anthropic, { u"Hi"_q }, u"zh"_q);
	const auto body = call.body.object();
	const auto messages = body.value(u"messages"_q).toArray();
	Require(body.value(u"model"_q) == u"stub"_q
		&& body.value(u"max_tokens"_q) == QJsonValue(4096)
		&& body.value(u"system"_q) == u"Be exact."_q
		&& body.value(u"temperature"_q) == QJsonValue(1.)
		&& messages.size() == 1
		&& messages[0].toObject().value(u"role"_q) == u"user"_q
		&& messages[0].toObject().value(u"content"_q).toString().contains(
			u"[\"Hi\"]"_q)
		&& call.query.isEmpty(),
		"Anthropic request body");
	anthropic.systemPrompt = QString();
	Require(!BuildTranslationCall(anthropic, { u"Hi"_q }, u"zh"_q)
		.body.object().contains(u"system"_q), "empty system sent");
	const auto headers = AuthHeaders(anthropic, "key");
	Require(headers.size() == 2
		&& headers[0].first == "x-api-key" && headers[0].second == "key"
		&& headers[1].first == "anthropic-version"
		&& headers[1].second == "2023-06-01",
		"Anthropic headers");
	const auto reply = [](const char *reason) {
		return QByteArray(R"({"content":[{"type":"thinking","thinking":"x"},)"
			R"({"type":"text","text":"[\"a\",\"b\"]"}],"stop_reason":")")
			+ reason + "\"}";
	};
	const auto parsed = ParseTranslationResult(anthropic, reply("end_turn"), 2);
	Require(parsed && (*parsed == QStringList{ u"a"_q, u"b"_q }),
		"Anthropic response");
	Require(!ParseTranslationResult(anthropic, reply("max_tokens"), 2),
		"truncated Anthropic response accepted");
	Require(!ParseTranslationResult(anthropic, reply("end_turn"), 3),
		"Anthropic length mismatch accepted");

	const auto accepts = [&](const QByteArray &content, const QByteArray &reason) {
		const auto result = ParseTranslationResult(
			service, OpenAiReply(content, reason), 2);
		return result && (*result == QStringList{ u"a"_q, u"b"_q });
	};
	Require(accepts(R"(["a","b"])", "stop"), "plain array");
	Require(accepts("```json\n[\"a\",\"b\"]\n```", "stop"), "code fence");
	Require(accepts("```\n[\"a\",\"b\"]\n```", "stop"), "bare code fence");
	Require(accepts("<think>\nplan [\"x\"]\n</think>\n\n[\"a\",\"b\"]", "stop"),
		"think prefix");
	Require(accepts("<think>x</think>```json\n[\"a\",\"b\"]\n```", "stop"),
		"think prefix with code fence");
	Require(accepts(R"(["a","b"])", QByteArray()), "null finish reason");
	Require(ParseTranslationResult(
			service,
			R"({"choices":[{"message":{"content":"[\"a\",\"b\"]",)"
			R"("reasoning_content":"ignored"}}]})",
			2).has_value(),
		"missing finish reason");
	Require(!accepts(R"(["a","b"])", "length"), "length accepted");
	Require(!accepts(R"(["a","b"])", "content_filter"), "filter accepted");
	Require(!accepts(R"(["a"])", "stop"), "short array accepted");
	Require(!accepts(R"(["a",""])", "stop"), "empty string accepted");
	Require(!accepts(R"(["a",1])", "stop"), "non-string accepted");
	Require(!accepts(R"({"a":"b"})", "stop"), "object accepted");
	Require(!accepts("<think>[\"a\",\"b\"]", "stop"), "open think accepted");
	Require(!accepts("```", "stop"), "empty fence accepted");
	std::cout << "PASS: Nagram LLM protocols and tolerant parsing" << std::endl;
}

void TestPresets() {
	using namespace Nagram;
	auto ids = std::set<std::string>();
	auto anthropic = 0;
	for (const auto &preset : ServicePresets()) {
		Require(ids.emplace(preset.id).second, "duplicate preset id");
		auto service = ServiceFromPreset(
			preset,
			u"00000000-0000-0000-0000-000000000001"_q,
			u"00000000-0000-0000-0000-000000000002"_q);
		Require(service.model.isEmpty(), "preset recommends a model");
		Require(service.baseUrl.scheme() == u"https"_q
			&& service.baseUrl.path().endsWith('/'), "preset address");
		Require(!service.name.isEmpty(), "preset name");
		if (LlmProtocol(service.protocol)) {
			service.model = u"model"_q;
		}
		Require(ParseService(SerializeService(service)).has_value(),
			"preset does not make a valid service");
		anthropic += (service.protocol == u"anthropic"_q) ? 1 : 0;
	}
	Require(ids.size() == 15 && anthropic == 1
		&& !ids.contains("yandex") && !ids.contains("openai-transcription"),
		"preset list includes removed services");
	std::cout << "PASS: Nagram service presets" << std::endl;
}

void TestContext(const Nagram::ServiceDefinition &service) {
	using namespace Nagram;
	const auto plain = [](const QString &text, qint64 topic = 0) {
		return ContextCandidate{ .text = text, .topic = topic };
	};
	Require(SelectContext({}, 0, false).isEmpty(), "empty history");
	auto before = std::vector<ContextCandidate>();
	for (auto i = 0; i != 10; ++i) {
		before.push_back(plain(u"m%1"_q.arg(i)));
	}
	Require(SelectContext(before, 0, false) == (QStringList{
			u"m4"_q, u"m5"_q, u"m6"_q, u"m7"_q, u"m8"_q, u"m9"_q }),
		"last six messages in order");
	Require(SelectContext(before, 0, true).isEmpty(),
		"context for a restricted message");

	before = {
		plain(u"keep"_q),
		ContextCandidate{ .text = u"service"_q, .regular = false },
		ContextCandidate{ .text = u"filtered"_q, .hidden = true },
		plain(u"other topic"_q, 7),
		plain(u"   "_q),
		plain(u"  last  "_q),
	};
	Require(SelectContext(before, 0, false)
			== (QStringList{ u"keep"_q, u"last"_q }),
		"service, hidden, empty and other topic messages skipped");
	Require(SelectContext(before, 7, false) == QStringList{ u"other topic"_q },
		"topic selection");
	before.push_back(ContextCandidate{ .text = u"secret"_q, .restricted = true });
	Require(SelectContext(before, 0, false).isEmpty(),
		"restricted message used as context");
	before.back().hidden = true;
	Require(SelectContext(before, 0, false).size() == 2,
		"skipped restricted message blocks the context");

	const auto longText = QString(kContextEach + 40, QChar('a'));
	Require(TruncateContext(longText, kContextEach).size() == kContextEach,
		"single message limit");
	const auto pair = QString::fromUtf8("\xF0\x9F\x98\x80");
	const auto surrogates = QString(kContextEach - 1, QChar('a')) + pair;
	const auto cut = TruncateContext(surrogates, kContextEach);
	Require(cut.size() == kContextEach - 1 && !cut.back().isHighSurrogate(),
		"surrogate pair split");
	Require(TruncateContext(u"short"_q, kContextEach) == u"short"_q,
		"short text changed");

	before.clear();
	for (auto i = 0; i != 6; ++i) {
		before.push_back(plain(QString(kContextEach, QChar('a' + i))));
	}
	const auto limited = SelectContext(before, 0, false);
	auto total = 0;
	for (const auto &text : limited) {
		total += text.size();
	}
	Require(limited.size() == kContextTotal / kContextEach
		&& total <= kContextTotal
		&& limited.back().front() == QChar('f')
		&& limited.front().front() == QChar('c'),
		"total limit drops the earliest messages");

	const auto context = QStringList{ u"first \"quoted\""_q, u"second"_q };
	const auto content = [&](const ServiceDefinition &value,
			const QStringList &list) {
		return BuildTranslationCall(value, { u"Hi"_q }, u"zh"_q, list)
			.body.object().value(u"messages"_q).toArray().last()
			.toObject().value(u"content"_q).toString();
	};
	const auto with = content(service, context);
	const auto block = u"<context>[\"first \\\"quoted\\\"\",\"second\"]</context>"_q;
	Require(with.contains(block)
		&& with.indexOf(block) < with.indexOf(u"Translate each string"_q)
		&& with.endsWith(u"[\"Hi\"]"_q)
		&& with.contains(u"do not translate"_q),
		"context precedes the strings to translate");
	Require(!content(service, {}).contains(u"<context>"_q)
		&& content(service, {}) == BuildTranslationCall(
			service, { u"Hi"_q }, u"zh"_q).body.object().value(
				u"messages"_q).toArray().last().toObject().value(
					u"content"_q).toString(),
		"request changed without context");
	auto anthropic = service;
	anthropic.protocol = u"anthropic"_q;
	Require(content(anthropic, context).contains(block),
		"Anthropic request without context");
	auto deepl = service;
	deepl.protocol = u"deepl"_q;
	deepl.model = QString();
	Require(BuildTranslationCall(deepl, { u"Hi"_q }, u"zh"_q, context).body
			== BuildTranslationCall(deepl, { u"Hi"_q }, u"zh"_q).body,
		"context sent to a non-LLM service");
	Require(!ParseTranslationResult(
			service,
			OpenAiReply(R"(["first","second","Hi"])", "stop"),
			1),
		"translated context accepted");
	std::cout << "PASS: Nagram translation context" << std::endl;
}

void TestSummary(const Nagram::ServiceDefinition &service) {
	using namespace Nagram;
	Require(PlanSummary({}).texts.isEmpty()
		&& PlanSummary({ u" "_q, QString() }).texts.isEmpty(),
		"empty summary scope");
	const auto single = PlanSummary({ u"  hello  "_q });
	Require(single.texts == QStringList{ u"hello"_q }
		&& single.characters == 5 && !single.truncated, "single message");
	const auto huge = PlanSummary({ QString(kSummaryLength + 10, QChar('a')) });
	Require(huge.texts.size() == 1 && huge.characters == kSummaryLength
		&& huge.truncated, "single message limit");
	auto many = QStringList();
	for (auto i = 0; i != kSummaryMessages + 5; ++i) {
		many.push_back(u"m%1"_q.arg(i));
	}
	const auto counted = PlanSummary(many);
	Require(counted.texts.size() == kSummaryMessages && counted.truncated
		&& counted.texts.front() == u"m5"_q
		&& counted.texts.back() == many.back(),
		"message count limit keeps the most recent");
	many.resize(kSummaryMessages);
	Require(!PlanSummary(many).truncated, "exact message count truncated");
	const auto half = QString(kSummaryLength / 2, QChar('b'));
	const auto sized = PlanSummary({ u"old"_q, half, u""_q, half });
	Require(sized.texts == (QStringList{ half, half })
		&& sized.characters == kSummaryLength && sized.truncated,
		"total limit drops the earliest");

	auto llm = service;
	llm.systemPrompt = u"System."_q;
	const auto body = BuildSummaryBody(llm, { u"a"_q, u"b \"q\""_q }, u"zh"_q);
	const auto messages = body.value(u"messages"_q).toArray();
	const auto content = messages.last().toObject().value(
		u"content"_q).toString();
	Require(messages.size() == 2
		&& messages[0].toObject().value(u"content"_q) == u"System."_q
		&& content.startsWith(u"Summarize the following"_q)
		&& content.contains(u"code zh."_q)
		&& content.endsWith(u"[\"a\",\"b \\\"q\\\"\"]"_q),
		"summary request");
	llm.summaryPrompt = u"Three bullet points."_q;
	llm.protocol = u"anthropic"_q;
	const auto custom = BuildSummaryBody(llm, { u"a"_q }, u"en"_q);
	Require(custom.value(u"system"_q) == u"System."_q
		&& custom.value(u"max_tokens"_q) == QJsonValue(4096)
		&& custom.value(u"messages"_q).toArray()[0].toObject().value(
			u"content"_q).toString().startsWith(u"Three bullet points."_q),
		"custom summary prompt");
	const auto summary = ParseSummaryResult(
		service, OpenAiReply("<think>x</think>  Short summary. ", "stop"));
	Require(summary && *summary == u"Short summary."_q, "summary reply");
	Require(!ParseSummaryResult(service, OpenAiReply("Cut off", "length")),
		"truncated summary accepted");
	Require(!ParseSummaryResult(service, OpenAiReply("  ", "stop")),
		"empty summary accepted");
	Require(!ParseSummaryResult(service, "{broken"), "broken reply accepted");
	Require(!ParseSummaryResult(service, OpenAiReply(
			QByteArray(kSummaryResultLength + 1, 'x'), "stop")),
		"oversized summary accepted");
	std::cout << "PASS: Nagram message summary" << std::endl;
}

void TestTranscriptionQueue() {
	using namespace Nagram;
	using Skip = TranscribeSkip;
	using Outcome = TranscribeOutcome;
	using Stop = TranscribeStop;
	const auto ready = TranscribeCandidate{
		.audio = true,
		.downloaded = true,
		.size = 1024,
	};
	Require(ClassifyTranscription(ready) == Skip::None, "ready audio skipped");
	Require(ClassifyTranscription({}) == Skip::NotAudio, "text message");
	auto candidate = ready;
	candidate.expiring = true;
	Require(ClassifyTranscription(candidate) == Skip::Expiring, "expiring");
	candidate = ready;
	candidate.cached = true;
	Require(ClassifyTranscription(candidate) == Skip::Cached, "cached");
	candidate = ready;
	candidate.downloaded = false;
	Require(ClassifyTranscription(candidate) == Skip::None,
		"undownloaded audio must enter the download/transcription queue");
	candidate = ready;
	candidate.size = kTranscribeMaxBytes + 1;
	Require(ClassifyTranscription(candidate) == Skip::TooLarge, "too large");
	candidate.size = kTranscribeMaxBytes;
	Require(ClassifyTranscription(candidate) == Skip::None, "size limit");
	candidate.size = 0;
	Require(ClassifyTranscription(candidate) == Skip::TooLarge, "empty file");

	auto cached = ready;
	cached.cached = true;
	const auto mixed = PlanTranscription({ ready, {}, cached, ready, {} });
	Require(mixed.accepted == (std::vector<int>{ 0, 3 }) && mixed.skipped == 1,
		"plan counts only audio messages");
	const auto many = PlanTranscription(std::vector<TranscribeCandidate>(
		kTranscribeBatchLimit + 4, ready));
	Require(int(many.accepted.size()) == kTranscribeBatchLimit
		&& many.skipped == 4 && many.accepted.back() == kTranscribeBatchLimit - 1,
		"batch limit");
	Require(PlanTranscription({}).accepted.empty(), "empty plan");

	auto empty = TranscriptionQueue();
	Require(!empty.current() && empty.stopped() == Stop::Finished,
		"empty queue is not finished");
	auto queue = TranscriptionQueue(3);
	Require(queue.current() == 0 && queue.stopped() == Stop::None, "start");
	queue.report(Outcome::Done);
	queue.report(Outcome::Failed);
	Require(queue.current() == 2 && queue.left() == 1, "continues on failure");
	queue.report(Outcome::Done);
	Require(!queue.current() && queue.stopped() == Stop::Finished
		&& queue.done() == 2 && queue.failed() == 1 && !queue.left(),
		"finished counts");
	queue.report(Outcome::Done);
	Require(queue.done() == 2, "report after the end counted");

	queue = TranscriptionQueue(5);
	queue.report(Outcome::Done);
	queue.report(Outcome::Fatal);
	Require(!queue.current() && queue.stopped() == Stop::Fatal
		&& queue.done() == 1 && queue.failed() == 1 && queue.left() == 3,
		"credential error does not stop the queue");

	queue = TranscriptionQueue(8);
	queue.report(Outcome::Network);
	queue.report(Outcome::Network);
	queue.report(Outcome::Done);
	queue.report(Outcome::Network);
	queue.report(Outcome::Failed);
	queue.report(Outcome::Network);
	queue.report(Outcome::Network);
	Require(queue.current() == 7, "separated network errors stopped the queue");
	queue = TranscriptionQueue(8);
	queue.report(Outcome::Done);
	for (auto i = 0; i != kTranscribeNetworkFailures; ++i) {
		Require(queue.current().has_value(), "stopped too early");
		queue.report(Outcome::Network);
	}
	Require(!queue.current() && queue.stopped() == Stop::Network
		&& queue.done() == 1 && queue.left() == 4,
		"three network errors in a row do not stop the queue");

	queue = TranscriptionQueue(4);
	queue.report(Outcome::Done);
	queue.report(Outcome::Changed);
	Require(queue.stopped() == Stop::Changed && queue.left() == 2,
		"changed configuration does not stop the queue");

	queue = TranscriptionQueue(4);
	queue.report(Outcome::Done);
	queue.cancel();
	queue.report(Outcome::Done);
	Require(!queue.current() && queue.stopped() == Stop::Cancelled
		&& queue.done() == 1 && queue.left() == 3,
		"cancel loses finished results or keeps running");
	std::cout << "PASS: Nagram batch transcription queue" << std::endl;
}

} // namespace

void TestServices() {
	using namespace Nagram;
	const auto service = ServiceDefinition{
		.id = u"00000000-0000-0000-0000-000000000001"_q,
		.name = u"Local translation"_q,
		.kind = ServiceKind::Translation,
		.protocol = u"openai"_q,
		.baseUrl = QUrl(u"http://127.0.0.1:18765/v1/"_q),
		.endpoint = u"chat/completions"_q,
		.model = u"stub"_q,
		.credentialRef = u"00000000-0000-0000-0000-000000000002"_q,
	};
	const auto serialized = SerializeService(service);
	Require(ParseService(serialized).has_value(), "valid local service rejected");
	Require(!serialized.contains(u"apiKey"_q)
		&& !serialized.contains(u"secret"_q), "secret serialized");
	auto config = ServicesDefaults();
	config.insert(u"instances"_q, QJsonArray{ serialized });
	config.insert(u"translation"_q, service.id);
	Require(ValidServices(config), "valid service selection rejected");
	Require(ValidServicesBytes(QJsonDocument(config).toJson()),
		"valid service bytes rejected");
	const auto account = CredentialAccount(service);
	auto changed = service;
	changed.endpoint = u"other"_q;
	Require(CredentialAccount(changed) != account,
		"credential binding ignored endpoint");
	auto invalid = serialized;
	invalid.insert(u"baseUrl"_q, u"http://example.com/v1/"_q);
	Require(!ParseService(invalid), "remote HTTP accepted");
	invalid = serialized;
	invalid.insert(u"endpoint"_q, u"../other"_q);
	Require(!ParseService(invalid), "path traversal accepted");
	invalid = serialized;
	invalid.insert(u"apiKey"_q, u"must-not-store"_q);
	Require(!ParseService(invalid), "secret field accepted");
	config.insert(u"transcription"_q, service.id);
	Require(!ValidServices(config), "cross-kind selection accepted");
	Require(!ValidServicesBytes("{broken"), "invalid JSON accepted");

	auto deepl = service;
	deepl.protocol = u"deepl"_q;
	deepl.model = u"prefer_less"_q;
	Require(ParseService(SerializeService(deepl)).has_value(),
		"DeepL formality rejected");
	deepl.model = u"formal"_q;
	Require(!ParseService(SerializeService(deepl)),
		"unknown DeepL formality accepted");
	deepl.model = u"more"_q;
	const auto deeplCall = BuildTranslationCall(deepl, { u"Hi"_q }, u"zh"_q);
	Require(deeplCall.body.object().value(u"formality"_q) == u"more"_q
		&& deeplCall.body.object().value(u"target_lang"_q) == u"ZH"_q,
		"DeepL request body");

	auto microsoft = service;
	microsoft.protocol = u"microsoft"_q;
	microsoft.model = u"eastasia"_q;
	Require(ParseService(SerializeService(microsoft)).has_value(),
		"Microsoft region rejected");
	microsoft.model = u"east asia"_q;
	Require(!ParseService(SerializeService(microsoft)),
		"invalid Microsoft region accepted");
	microsoft.model = u"eastasia"_q;
	const auto microsoftCall = BuildTranslationCall(
		microsoft,
		{ u"A"_q, u"B"_q },
		u"ja"_q);
	Require(microsoftCall.body.isArray()
		&& microsoftCall.body.array().size() == 2
		&& microsoftCall.query.queryItemValue(u"to"_q) == u"ja"_q
		&& microsoftCall.query.queryItemValue(u"api-version"_q) == u"3.0"_q,
		"Microsoft request");
	const auto microsoftResult = ParseTranslationResult(
		microsoft,
		R"([{"translations":[{"text":"a"}]},{"translations":[{"text":"b"}]}])",
		2);
	Require(microsoftResult
		&& (*microsoftResult == QStringList{ u"a"_q, u"b"_q }),
		"Microsoft response");
	const auto headers = AuthHeaders(microsoft, "key");
	Require(headers.size() == 2
		&& headers[0].first == "Ocp-Apim-Subscription-Key"
		&& headers[1].second == "eastasia",
		"Microsoft headers");

	auto google = service;
	google.protocol = u"google"_q;
	google.model = QString();
	Require(ParseService(SerializeService(google)).has_value(),
		"Google service rejected");
	const auto googleResult = ParseTranslationResult(
		google,
		R"({"data":{"translations":[{"translatedText":"x"}]}})",
		1);
	Require(googleResult && googleResult->front() == u"x"_q,
		"Google response");
	Require(AuthHeaders(google, "k").front().first == "X-goog-api-key",
		"Google header");

	auto yandex = service;
	yandex.id = u"d0a9a4bf-dc8e-4c65-9a9a-f73d21f887ce"_q;
	yandex.protocol = u"yandex"_q;
	yandex.model = u"b1gabc"_q;
	Require(!ParseService(SerializeService(yandex)), "retired service accepted");
	auto legacy = ServicesDefaults();
	legacy.insert(u"instances"_q, QJsonArray{
		SerializeService(google),
		SerializeService(yandex),
	});
	legacy.insert(u"translation"_q, yandex.id);
	const auto upgraded = UpgradeServices(legacy);
	Require(ValidServices(legacy)
		&& upgraded.value(u"translation"_q).toString().isEmpty()
		&& upgraded.value(u"instances"_q).toArray().size() == 1
		&& FindService(upgraded, google.id).has_value(),
		"retirement damaged retained services");
	auto googleTranscription = google;
	googleTranscription.kind = ServiceKind::Transcription;
	Require(!ParseService(SerializeService(googleTranscription)),
		"non-OpenAI transcription accepted");
	std::cout << "PASS: Nagram service config and credential binding" << std::endl;
	TestServicesV2(service);
	TestLlmProtocols(service);
	TestPresets();
	TestContext(service);
	TestSummary(service);
	TestTranscriptionQueue();
}
