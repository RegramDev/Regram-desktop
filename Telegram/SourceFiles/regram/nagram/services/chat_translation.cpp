#include "regram/nagram/services/chat_translation.h"

#include "base/timer.h"
#include "core/application.h"
#include "data/data_changes.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "regram/nagram/services/chat_translation_model.h"
#include "regram/nagram/services/request.h"
#include "regram/nagram/services/translation.h"
#include "regram/ayu/ayu_settings.h"
#include "platform/platform_translate_provider.h"
#include "window/window_controller.h"

namespace Nagram {
namespace {

constexpr auto kRetryDelay = crl::time(2000);

void Notify(const QString &key, const QString &text) {
	static auto throttle = ChatErrorThrottle();
	if (!throttle.allow(key, crl::now())) {
		return;
	} else if (const auto window = Core::App().activeWindow()) {
		window->showToast(text);
	}
}

class ChatTranslateProvider final : public Ui::TranslateProvider {
public:
	explicit ChatTranslateProvider(not_null<History*> history);

	bool supportsMessageId() const override;
	void request(
		Ui::TranslateProviderRequest request,
		LanguageId to,
		Fn<void(Ui::TranslateProviderResult)> done) override;
	void requestBatch(
		std::vector<Ui::TranslateProviderRequest> requests,
		const LanguageId &to,
		Fn<void(int, Ui::TranslateProviderResult)> doneOne,
		Fn<void()> doneAll) override;
	void cancel();

private:
	struct Failure {
		ServiceError error = ServiceError::None;
		int status = 0;
	};

	[[nodiscard]] ChatTranslationMode mode() const;
	void send(QStringList texts, Fn<void(ChatSendResult)> done);
	void finished();

	const not_null<Main::Session*> _session;
	std::unique_ptr<Ui::TranslateProvider> _upstream;
	std::unique_ptr<Ui::TranslateProvider> _system;
	ServiceRequest _request;
	base::Timer _retry;
	Fn<void()> _retryCallback;
	ChatTranslationQueue _queue;
	std::optional<ServiceDefinition> _service;
	std::vector<std::optional<TranslationPlan>> _plans;
	QString _to;
	Failure _failure;
	rpl::lifetime _lifetime;

};

ChatTranslateProvider::ChatTranslateProvider(not_null<History*> history)
: _session(&history->session())
, _upstream(CreateInteractiveTranslateProvider(_session, [](QString error) {
	Notify(u"chat-translation"_q, error);
}))
, _retry([=] {
	if (const auto callback = base::take(_retryCallback)) {
		callback();
	}
})
, _queue([=](QStringList texts, Fn<void(ChatSendResult)> done) {
	send(std::move(texts), std::move(done));
}, [=](Fn<void()> callback) {
	_retryCallback = std::move(callback);
	_retry.callOnce(kRetryDelay);
}) {
	history->session().changes().historyUpdates(
		history,
		Data::HistoryUpdate::Flag::TranslatedTo
	) | rpl::on_next([=] {
		_queue.resume();
	}, _lifetime);
}

ChatTranslationMode ChatTranslateProvider::mode() const {
	const auto settings = Services();
	return ChooseChatTranslationMode(
		true,
		settings.has_value(),
		settings ? settings->value(u"translation"_q).toString() : QString());
}

bool ChatTranslateProvider::supportsMessageId() const {
	return (mode() == ChatTranslationMode::Upstream)
		&& _upstream->supportsMessageId();
}

void ChatTranslateProvider::request(
		Ui::TranslateProviderRequest request,
		LanguageId to,
		Fn<void(Ui::TranslateProviderResult)> done) {
	if (mode() == ChatTranslationMode::Upstream) {
		_upstream->request(std::move(request), to, std::move(done));
		return;
	}
	auto requests = std::vector<Ui::TranslateProviderRequest>();
	requests.push_back(std::move(request));
	requestBatch(
		std::move(requests),
		to,
		[done = std::move(done)](int, Ui::TranslateProviderResult result) {
			done(std::move(result));
		},
		[] {});
}

void ChatTranslateProvider::requestBatch(
		std::vector<Ui::TranslateProviderRequest> requests,
		const LanguageId &to,
		Fn<void(int, Ui::TranslateProviderResult)> doneOne,
		Fn<void()> doneAll) {
	const auto chosen = mode();
	if (chosen == ChatTranslationMode::Upstream) {
		_upstream->requestBatch(
			std::move(requests),
			to,
			std::move(doneOne),
			std::move(doneAll));
		return;
	}
	const auto failAll = [&](const QString &key, const QString &text) {
		Notify(key, text);
		for (auto i = 0; i != int(requests.size()); ++i) {
			doneOne(i, { .error = Ui::TranslateProviderError::Unknown });
		}
		doneAll();
	};
	if (chosen == ChatTranslationMode::System) {
		if (!Platform::IsTranslateProviderAvailable()) {
			failAll(
				u"system"_q,
				tr::lng_nagram_system_translation_unavailable(tr::now));
			return;
		} else if (!_system) {
			_system = PreserveTranslationEntities(Platform::CreateTranslateProvider());
		}
		_system->requestBatch(
			std::move(requests),
			to,
			std::move(doneOne),
			std::move(doneAll));
		return;
	}
	const auto settings = Services();
	_service = settings
		? FindService(*settings, settings->value(u"translation"_q).toString())
		: std::nullopt;
	if (!_service || !to.known()) {
		failAll(
			u"configuration"_q,
			ServiceErrorText(ServiceError::Configuration));
		return;
	}
	_to = to.twoLetterCode();
	_failure = {};
	_plans.clear();
	auto messages = std::vector<ChatMessageTexts>();
	for (auto &request : requests) {
		_plans.push_back(PlanTranslation(std::move(request.text)));
		messages.push_back(_plans.back()
			? ChatMessageTexts(_plans.back()->texts)
			: std::nullopt);
	}
	_queue.start(std::move(messages), [=](int index, ChatMessageTexts texts) {
		auto result = (texts && _plans[index])
			? ApplyTranslation(*_plans[index], *texts)
			: std::nullopt;
		doneOne(index, result
			? Ui::TranslateProviderResult{ .text = std::move(*result) }
			: Ui::TranslateProviderResult{
				.error = Ui::TranslateProviderError::Unknown,
			});
	}, [=] {
		finished();
		doneAll();
	});
}

void ChatTranslateProvider::send(
		QStringList texts,
		Fn<void(ChatSendResult)> done) {
	const auto call = BuildTranslationCall(*_service, texts, _to);
	const auto amount = int(texts.size());
	_request.json(*_service, call.body, call.query, [=](
			ServiceResult response) {
		auto values = (response.error == ServiceError::None)
			? ParseTranslationResult(*_service, response.body, amount)
			: std::nullopt;
		_failure = values
			? Failure()
			: (response.error == ServiceError::None)
			? Failure{ ServiceError::Response }
			: Failure{ response.error, response.status };
		done({
			.texts = std::move(values),
			.network = (response.error == ServiceError::Network),
		});
	});
}

void ChatTranslateProvider::finished() {
	if (!_service) {
		return;
	}
	const auto failure = base::take(_failure);
	if (failure.error != ServiceError::None) {
		Notify(
			u"%1:%2:%3"_q.arg(_service->id).arg(int(failure.error)).arg(
				failure.status),
			ServiceErrorText(failure.error, failure.status));
	}
	if (_queue.paused()) {
		Notify(
			_service->id + u":paused"_q,
			tr::lng_nagram_chat_translation_paused(
				tr::now,
				lt_name,
				_service->name));
	}
}

void ChatTranslateProvider::cancel() {
	_queue.cancel();
	_request.cancel();
	_retry.cancel();
	_retryCallback = nullptr;
	_failure = {};
	_system = nullptr;
	_upstream = CreateInteractiveTranslateProvider(_session, [](QString error) {
		Notify(u"chat-translation"_q, error);
	});
}

} // namespace

std::unique_ptr<Ui::TranslateProvider> CreateChatTranslateProvider(
		not_null<History*> history) {
	return std::make_unique<ChatTranslateProvider>(history);
}

bool ChatTranslationServiceActive() {
	const auto settings = Services();
	if (!settings) {
		return false;
	}
	const auto selected = settings->value(u"translation"_q).toString();
	switch (ChooseChatTranslationMode(true, true, selected)) {
	case ChatTranslationMode::Upstream:
		if (selected == u"telegram"_q) {
			return false;
		}
		switch (AyuSettings::getInstance().translationProvider()) {
		case TranslationProvider::Google: return true;
		case TranslationProvider::Native:
			return Platform::IsTranslateProviderAvailable();
		case TranslationProvider::Telegram: return false;
		}
		return false;
	case ChatTranslationMode::System:
		return Platform::IsTranslateProviderAvailable();
	case ChatTranslationMode::External:
		return FindService(*settings, selected).has_value();
	}
	Unexpected("Invalid chat translation mode.");
}

void CancelChatTranslation(Ui::TranslateProvider *provider) {
	if (const auto chat = dynamic_cast<ChatTranslateProvider*>(provider)) {
		chat->cancel();
	}
}

} // namespace Nagram
