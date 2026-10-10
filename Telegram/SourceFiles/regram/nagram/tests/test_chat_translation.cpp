#include "regram/nagram/services/chat_translation_model.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/core/options.h"
#include "base/algorithm.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <stdexcept>

namespace {

using namespace Nagram;

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

struct Harness {
	Harness()
	: queue([=](QStringList texts, Fn<void(ChatSendResult)> done) {
		sent.push_back(std::move(texts));
		reply = std::move(done);
	}, [=](Fn<void()> callback) {
		++delays;
		delayed = std::move(callback);
	}) {
	}

	void start(std::vector<ChatMessageTexts> messages) {
		results.assign(messages.size(), std::nullopt);
		calls.assign(messages.size(), 0);
		finished = 0;
		queue.start(std::move(messages), [=](int index, ChatMessageTexts value) {
			++calls[index];
			results[index] = std::move(value);
		}, [=] {
			++finished;
		});
	}
	void succeed() {
		auto translated = QStringList();
		for (const auto &text : sent.back()) {
			translated.push_back(u"T:"_q + text);
		}
		base::take(reply)({ .texts = std::move(translated) });
	}
	void failWith(bool network) {
		base::take(reply)({ .network = network });
	}
	[[nodiscard]] bool everyoneOnce() const {
		for (const auto &count : calls) {
			if (count != 1) {
				return false;
			}
		}
		return finished == 1;
	}

	std::vector<QStringList> sent;
	Fn<void(ChatSendResult)> reply;
	Fn<void()> delayed;
	int delays = 0;
	std::vector<ChatMessageTexts> results;
	std::vector<int> calls;
	int finished = 0;
	ChatTranslationQueue queue;
};

void TestMode() {
	using Mode = ChatTranslationMode;
	for (const auto &selected : {
			QString(),
			u"telegram"_q,
			u"system"_q,
			u"00000000-0000-0000-0000-000000000001"_q }) {
		Require(ChooseChatTranslationMode(false, true, selected)
			== Mode::Upstream, "disabled option leaves upstream");
		Require(ChooseChatTranslationMode(false, false, selected)
			== Mode::Upstream, "disabled option with bad config");
	}
	Require(ChooseChatTranslationMode(true, true, QString())
		== Mode::Upstream, "inherit stays upstream");
	Require(ChooseChatTranslationMode(true, true, u"telegram"_q)
		== Mode::Upstream, "telegram stays upstream");
	Require(ChooseChatTranslationMode(true, true, u"system"_q)
		== Mode::System, "system translation");
	Require(ChooseChatTranslationMode(true, true, u"id"_q)
		== Mode::External, "external instance");
	Require(ChooseChatTranslationMode(true, false, QString())
		== Mode::External, "invalid config falls back to Telegram");

	auto registry = Registry();
	RegisterServiceOptions(registry);
	const auto quick = registry.Find(kQuickTranslateButton.key);
	Require(quick && quick->scope == Scope::Device
		&& quick->fallbackRaw == "0"
		&& (quick->flags & static_cast<unsigned>(Flag::RefreshMessageView)),
		"quick translation registered with a message layout refresh");
	const auto info = registry.Find(kChatTranslationUseService.key);
	Require(info && info->scope == Scope::Device
		&& info->fallbackRaw == "0"
		&& (info->flags & static_cast<unsigned>(Flag::Exportable)),
		"whole-chat option registration");
}

void TestSplit() {
	Require(SplitChatBatch({}).requests.empty(), "empty batch");
	auto messages = std::vector<ChatMessageTexts>();
	auto parts = 0;
	for (auto i = 0; i != 20; ++i) {
		auto texts = QStringList();
		for (auto j = 0; j != (i % 7); ++j) {
			texts.push_back(u"m%1p%2"_q.arg(i).arg(j));
			++parts;
		}
		messages.push_back(texts);
	}
	const auto plan = SplitChatBatch(messages);
	auto seen = std::vector<QStringList>(messages.size());
	auto total = 0;
	for (const auto &request : plan.requests) {
		Require(!request.texts.isEmpty()
			&& request.texts.size() <= kChatBatchParts
			&& request.texts.size() == int(request.messages.size()),
			"request part limit");
		for (auto i = 0; i != request.texts.size(); ++i) {
			seen[request.messages[i]].push_back(request.texts[i]);
			++total;
		}
	}
	Require(total == parts && plan.rejected.empty()
		&& plan.requests.size() == 2, "all parts sent exactly once");
	for (auto i = 0; i != int(messages.size()); ++i) {
		Require(seen[i] == *messages[i], "part order per message");
	}

	const auto chunk = QString(4000, QChar(0x4E2D));
	auto large = std::vector<ChatMessageTexts>();
	for (auto i = 0; i != 12; ++i) {
		large.push_back(QStringList{ chunk });
	}
	large.push_back(QStringList{ chunk, chunk });
	large.push_back(std::nullopt);
	large.push_back(QStringList{ u"small"_q });
	const auto sized = SplitChatBatch(large);
	Require(sized.rejected == (std::vector<int>{ 12, 13 }),
		"oversized and unplanned messages rejected");
	Require(sized.requests.size() == 2, "byte limit splits requests");
	for (const auto &request : sized.requests) {
		auto list = QJsonArray();
		for (const auto &text : request.texts) {
			list.push_back(text);
		}
		Require(QJsonDocument(list).toJson(QJsonDocument::Compact).size()
			<= kChatBatchBytes, "request byte limit");
		for (const auto &index : request.messages) {
			Require(index != 12 && index != 13, "rejected message sent");
		}
	}
}

void TestQueue() {
	{
		auto harness = Harness();
		harness.start({
			QStringList{ u"a"_q, u"b"_q },
			QStringList(),
			std::nullopt,
			QStringList{ u"c"_q },
		});
		Require(harness.sent.size() == 1
			&& harness.sent.back() == (QStringList{ u"a"_q, u"b"_q, u"c"_q })
			&& harness.calls[1] == 1
			&& harness.results[1] && harness.results[1]->isEmpty()
			&& !harness.finished, "first request");
		harness.succeed();
		Require(harness.everyoneOnce()
			&& *harness.results[0] == (QStringList{ u"T:a"_q, u"T:b"_q })
			&& !harness.results[2]
			&& *harness.results[3] == QStringList{ u"T:c"_q }
			&& !harness.queue.active(), "successful batch");
	}
	{
		auto harness = Harness();
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(true);
		Require(harness.delays == 1 && !harness.finished
			&& harness.sent.size() == 1, "network error schedules one retry");
		base::take(harness.delayed)();
		Require(harness.sent.size() == 2, "retry sent");
		harness.succeed();
		Require(harness.everyoneOnce() && harness.results[0].has_value()
			&& !harness.queue.paused(), "retry succeeded");

		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(true);
		base::take(harness.delayed)();
		harness.failWith(true);
		Require(harness.everyoneOnce() && !harness.results[0]
			&& harness.delays == 2 && harness.sent.size() == 4,
			"second network error fails the batch");
	}
	{
		auto harness = Harness();
		harness.start({ QStringList{ u"a"_q }, QStringList{ u"b"_q } });
		harness.failWith(false);
		Require(harness.everyoneOnce() && !harness.delays
			&& !harness.results[0] && !harness.results[1]
			&& harness.sent.size() == 1, "other errors are not retried");
		base::take(harness.reply);
		harness.start({ QStringList{ u"a"_q } });
		base::take(harness.reply)({ .texts = QStringList{ u"x"_q, u"y"_q } });
		Require(harness.everyoneOnce() && !harness.results[0],
			"wrong reply length accepted");
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(false);
		Require(harness.queue.paused() && harness.sent.size() == 3,
			"three failed batches open the breaker");
		harness.start({ QStringList{ u"a"_q }, QStringList() });
		Require(harness.everyoneOnce() && harness.sent.size() == 3
			&& !harness.results[0] && !harness.results[1],
			"paused queue still sends");
		harness.queue.resume();
		harness.start({ QStringList{ u"a"_q } });
		Require(harness.sent.size() == 4, "resumed queue does not send");
		harness.succeed();
		Require(harness.everyoneOnce() && !harness.queue.paused(), "resumed");
	}
	{
		auto harness = Harness();
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(false);
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(false);
		harness.start({ QStringList{ u"a"_q } });
		harness.succeed();
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(false);
		Require(!harness.queue.paused(), "success did not reset the breaker");
	}
	{
		auto harness = Harness();
		harness.start({ QStringList{ u"a"_q } });
		harness.queue.cancel();
		harness.succeed();
		Require(!harness.calls[0] && !harness.finished
			&& !harness.queue.active(), "result delivered after cancel");
		harness.start({ QStringList{ u"a"_q } });
		harness.failWith(true);
		harness.queue.cancel();
		base::take(harness.delayed)();
		Require(harness.sent.size() == 2 && !harness.finished,
			"retry sent after cancel");
	}
	{
		auto harness = Harness();
		harness.start({ QStringList{ u"a"_q }, QStringList{ u"b"_q } });
		harness.queue.fail();
		Require(harness.everyoneOnce() && !harness.results[0]
			&& !harness.queue.active() && !harness.queue.paused(),
			"failing the active batch");
		harness.succeed();
		Require(harness.everyoneOnce(), "late reply after fail");
		harness.queue.fail();
		Require(harness.finished == 1, "fail without a batch");
	}
	{
		auto parts = QStringList();
		for (auto i = 0; i != kChatBatchParts + 1; ++i) {
			parts.push_back(u"p%1"_q.arg(i));
		}
		auto harness = Harness();
		harness.start({ parts, QStringList{ u"z"_q } });
		Require(harness.sent.back().size() == kChatBatchParts, "first chunk");
		harness.succeed();
		Require(!harness.calls[0] && !harness.finished
			&& harness.sent.size() == 2
			&& harness.sent.back() == (QStringList{ u"p50"_q, u"z"_q }),
			"second chunk");
		harness.failWith(false);
		Require(harness.everyoneOnce() && !harness.results[0]
			&& !harness.results[1], "partial message reported as done");
	}
}

void TestThrottle() {
	auto throttle = ChatErrorThrottle();
	Require(throttle.allow(u"a"_q, 1000), "first error hidden");
	Require(!throttle.allow(u"a"_q, 1000 + kChatErrorInterval - 1),
		"repeated error shown");
	Require(throttle.allow(u"b"_q, 2000), "other error hidden");
	Require(throttle.allow(u"a"_q, 1000 + kChatErrorInterval),
		"error hidden after the interval");
}

} // namespace

void TestChatTranslation() {
	TestMode();
	TestSplit();
	TestQueue();
	TestThrottle();
	std::cout << "PASS: Nagram whole-chat translation batches" << std::endl;
}
