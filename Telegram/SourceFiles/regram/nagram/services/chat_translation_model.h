#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <map>
#include <optional>
#include <vector>

namespace Nagram {

inline constexpr auto kChatBatchParts = 50;
inline constexpr auto kChatBatchBytes = 96 * 1024;
inline constexpr auto kChatMessageBytes = 16 * 1024;
inline constexpr auto kChatBreakerFailures = 3;
inline constexpr auto kChatErrorInterval = 30 * 1000;

enum class ChatTranslationMode {
	Upstream,
	System,
	External,
};

[[nodiscard]] ChatTranslationMode ChooseChatTranslationMode(
	bool enabled,
	bool valid,
	const QString &selected);

using ChatMessageTexts = std::optional<QStringList>;

struct ChatBatchRequest {
	QStringList texts;
	std::vector<int> messages;
};

struct ChatBatchPlan {
	std::vector<ChatBatchRequest> requests;
	std::vector<int> rejected;
};

[[nodiscard]] ChatBatchPlan SplitChatBatch(
	const std::vector<ChatMessageTexts> &messages);

struct ChatSendResult {
	std::optional<QStringList> texts;
	bool network = false;
};

class ChatTranslationQueue final {
public:
	using Send = Fn<void(QStringList, Fn<void(ChatSendResult)>)>;
	using Delay = Fn<void(Fn<void()>)>;
	using DoneOne = Fn<void(int, ChatMessageTexts)>;

	ChatTranslationQueue(Send send, Delay delay);

	void start(
		std::vector<ChatMessageTexts> messages,
		DoneOne doneOne,
		Fn<void()> doneAll);
	void cancel();
	void fail();
	void resume();

	[[nodiscard]] bool active() const {
		return _active;
	}
	[[nodiscard]] bool paused() const {
		return _failures >= kChatBreakerFailures;
	}

private:
	void sendCurrent();
	void received(ChatSendResult result);
	void finish(bool failed);

	const Send _send;
	const Delay _delay;
	ChatBatchPlan _plan;
	std::vector<QStringList> _translated;
	std::vector<int> _pending;
	DoneOne _doneOne;
	Fn<void()> _doneAll;
	int _index = 0;
	int _generation = 0;
	int _failures = 0;
	bool _retried = false;
	bool _active = false;

};

class ChatErrorThrottle final {
public:
	[[nodiscard]] bool allow(const QString &key, qint64 now);

private:
	std::map<QString, qint64> _shown;

};

} // namespace Nagram
