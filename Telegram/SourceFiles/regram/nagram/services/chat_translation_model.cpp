#include "regram/nagram/services/chat_translation_model.h"

#include "base/algorithm.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Nagram {
namespace {

[[nodiscard]] int SerializedSize(const QString &text) {
	return QJsonDocument(QJsonArray{ text }).toJson(
		QJsonDocument::Compact).size() - 1;
}

} // namespace

ChatTranslationMode ChooseChatTranslationMode(
		bool enabled,
		bool valid,
		const QString &selected) {
	return !enabled
		? ChatTranslationMode::Upstream
		: !valid
		? ChatTranslationMode::External
		: (selected.isEmpty() || selected == u"telegram"_q)
		? ChatTranslationMode::Upstream
		: (selected == u"system"_q)
		? ChatTranslationMode::System
		: ChatTranslationMode::External;
}

ChatBatchPlan SplitChatBatch(const std::vector<ChatMessageTexts> &messages) {
	auto result = ChatBatchPlan();
	auto bytes = 0;
	for (auto i = 0; i != int(messages.size()); ++i) {
		auto sizes = std::vector<int>();
		auto total = 0;
		if (messages[i]) {
			for (const auto &text : *messages[i]) {
				sizes.push_back(SerializedSize(text));
				total += sizes.back();
			}
		}
		if (!messages[i] || total > kChatMessageBytes) {
			result.rejected.push_back(i);
			continue;
		}
		for (auto part = 0; part != int(sizes.size()); ++part) {
			if (result.requests.empty()
				|| result.requests.back().texts.size() == kChatBatchParts
				|| bytes + sizes[part] > kChatBatchBytes) {
				result.requests.emplace_back();
				bytes = 0;
			}
			bytes += sizes[part];
			result.requests.back().texts.push_back((*messages[i])[part]);
			result.requests.back().messages.push_back(i);
		}
	}
	return result;
}

ChatTranslationQueue::ChatTranslationQueue(Send send, Delay delay)
: _send(std::move(send))
, _delay(std::move(delay)) {
}

void ChatTranslationQueue::start(
		std::vector<ChatMessageTexts> messages,
		DoneOne doneOne,
		Fn<void()> doneAll) {
	fail();
	++_generation;
	_active = true;
	_plan = paused() ? ChatBatchPlan() : SplitChatBatch(messages);
	_translated.assign(messages.size(), QStringList());
	_pending.assign(messages.size(), 0);
	_doneOne = std::move(doneOne);
	_doneAll = std::move(doneAll);
	_index = 0;
	_retried = false;
	if (paused()) {
		for (auto i = 0; i != int(messages.size()); ++i) {
			_pending[i] = -1;
		}
		finish(false);
		return;
	}
	for (const auto &index : _plan.rejected) {
		_pending[index] = -1;
	}
	for (const auto &request : _plan.requests) {
		for (const auto &index : request.messages) {
			++_pending[index];
		}
	}
	const auto generation = _generation;
	for (auto i = 0; i != int(_pending.size()); ++i) {
		if (!_pending[i]) {
			_doneOne(i, QStringList());
			if (generation != _generation) {
				return;
			}
		}
	}
	sendCurrent();
}

void ChatTranslationQueue::sendCurrent() {
	if (_index == int(_plan.requests.size())) {
		finish(false);
		return;
	}
	const auto generation = _generation;
	_send(_plan.requests[_index].texts, [=](ChatSendResult result) {
		if (_active && generation == _generation) {
			received(std::move(result));
		}
	});
}

void ChatTranslationQueue::received(ChatSendResult result) {
	const auto &request = _plan.requests[_index];
	const auto generation = _generation;
	if (result.texts && result.texts->size() == request.texts.size()) {
		for (auto i = 0; i != int(request.messages.size()); ++i) {
			const auto index = request.messages[i];
			_translated[index].push_back((*result.texts)[i]);
			if (!--_pending[index]) {
				_doneOne(index, _translated[index]);
				if (generation != _generation) {
					return;
				}
			}
		}
		++_index;
		_retried = false;
		sendCurrent();
	} else if (result.network && !_retried) {
		_retried = true;
		_delay([=] {
			if (_active && generation == _generation) {
				sendCurrent();
			}
		});
	} else {
		finish(true);
	}
}

void ChatTranslationQueue::finish(bool failed) {
	const auto generation = _generation;
	if (failed) {
		++_failures;
	} else if (!_plan.requests.empty()) {
		_failures = 0;
	}
	for (auto i = 0; i != int(_pending.size()); ++i) {
		if (_pending[i]) {
			_pending[i] = 0;
			_doneOne(i, std::nullopt);
			if (generation != _generation) {
				return;
			}
		}
	}
	_active = false;
	const auto done = base::take(_doneAll);
	_doneOne = nullptr;
	done();
}

void ChatTranslationQueue::cancel() {
	++_generation;
	_active = false;
	_doneOne = nullptr;
	_doneAll = nullptr;
}

void ChatTranslationQueue::fail() {
	if (_active) {
		const auto failures = _failures;
		finish(true);
		_failures = failures;
	}
}

void ChatTranslationQueue::resume() {
	_failures = 0;
}

bool ChatErrorThrottle::allow(const QString &key, qint64 now) {
	const auto i = _shown.find(key);
	if (i != _shown.end() && now - i->second < kChatErrorInterval) {
		return false;
	}
	_shown[key] = now;
	return true;
}

} // namespace Nagram
