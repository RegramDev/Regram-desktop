#include "regram/nagram/services/translation.h"

#include "core/application.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "lang/translate_mtproto_provider.h"
#include "lang/translate_provider.h"
#include "platform/platform_translate_provider.h"
#include "regram/nagram/services/context.h"
#include "regram/nagram/services/request.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Nagram {
namespace {

class ExternalTranslateProvider final : public Ui::TranslateProvider {
public:
	ExternalTranslateProvider(
		std::optional<ServiceDefinition> service,
		Fn<void(QString)> error,
		QString unavailable = QString(),
		QStringList context = QStringList())
	: _service(std::move(service))
	, _error(std::move(error))
	, _unavailable(std::move(unavailable))
	, _context(std::move(context)) {
	}

	bool supportsMessageId() const override {
		return false;
	}

	void request(
			Ui::TranslateProviderRequest request,
			LanguageId to,
			Fn<void(Ui::TranslateProviderResult)> done) override {
		_request.cancel();
		if (!_service) {
			fail(ServiceError::Configuration, 0, std::move(done));
			return;
		}
		auto plan = PlanTranslation(std::move(request.text));
		if (!plan || !to.known()) {
			fail(ServiceError::Response, 0, std::move(done));
			return;
		}
		_plan = std::move(*plan);
		_translated.clear();
		_to = to.twoLetterCode();
		_done = std::move(done);
		next();
	}

private:
	void fail(ServiceError error, int status, Fn<void(Ui::TranslateProviderResult)> done) {
		const auto notify = _error;
		notify(_unavailable.isEmpty()
			? ServiceErrorText(error, status) : _unavailable);
		done({ .error = Ui::TranslateProviderError::Unknown });
	}

	void next() {
		const auto offset = int(_translated.size());
		if (offset == _plan.texts.size()) {
			const auto result = ApplyTranslation(_plan, _translated);
			const auto done = std::move(_done);
			if (!result) {
				fail(ServiceError::Response, 0, done);
			} else {
				done({ .text = *result });
			}
			return;
		}
		const auto amount = std::min(50, int(_plan.texts.size()) - offset);
		auto texts = QStringList();
		for (auto i = 0; i != amount; ++i) {
			texts.push_back(_plan.texts[offset + i]);
		}
		auto call = BuildTranslationCall(*_service, texts, _to, _context);
		_request.json(*_service, call.body, call.query, [=](ServiceResult response) {
			if (response.error != ServiceError::None) {
				fail(response.error, response.status, std::move(_done));
				return;
			}
			const auto values = ParseTranslationResult(
				*_service,
				response.body,
				amount);
			if (!values) {
				fail(ServiceError::Response, 0, std::move(_done));
				return;
			}
			_translated.append(*values);
			next();
		});
	}

	std::optional<ServiceDefinition> _service;
	Fn<void(QString)> _error;
	QString _unavailable;
	QStringList _context;
	Fn<void(Ui::TranslateProviderResult)> _done;
	ServiceRequest _request;
	TranslationPlan _plan;
	QStringList _translated;
	QString _to;

};

} // namespace

std::unique_ptr<Ui::TranslateProvider> CreateServiceTranslateProvider(
		const ServiceDefinition &service,
		Fn<void(QString)> error) {
	return std::make_unique<ExternalTranslateProvider>(
		service.kind == ServiceKind::Translation
			&& ParseService(SerializeService(service))
			? std::optional(service) : std::nullopt,
		std::move(error));
}

std::unique_ptr<Ui::TranslateProvider> CreateInteractiveTranslateProvider(
		not_null<Main::Session*> session,
		Fn<void(QString)> error) {
	const auto settings = Services();
	if (settings) {
		const auto id = settings->value(u"translation"_q).toString();
		if (id.isEmpty()) {
			return Ui::CreateTranslateProvider(session);
		} else if (id == u"telegram"_q) {
			return Ui::CreateMTProtoTranslateProvider(session);
		} else if (id == u"system"_q) {
			if (Platform::IsTranslateProviderAvailable()) {
				return Platform::CreateTranslateProvider();
			}
			return std::make_unique<ExternalTranslateProvider>(
					std::nullopt, std::move(error),
					tr::lng_nagram_system_translation_unavailable(tr::now));
		}
		return std::make_unique<ExternalTranslateProvider>(
			FindService(*settings, id), std::move(error));
	}
	return std::make_unique<ExternalTranslateProvider>(std::nullopt, std::move(error));
}

std::unique_ptr<Ui::TranslateProvider> CreateMessageTranslateProvider(
		not_null<PeerData*> peer,
		MsgId msgId,
		bool hasCopyRestriction,
		Fn<void(QString)> error) {
	const auto settings = ForDevice().Get(kTranslationContext)
		? Services()
		: std::nullopt;
	const auto service = (settings && !hasCopyRestriction)
		? FindService(*settings, settings->value(u"translation"_q).toString())
		: std::nullopt;
	const auto item = (service && LlmProtocol(service->protocol))
		? peer->owner().message(peer->id, msgId)
		: nullptr;
	auto context = (item && item->isRegular())
		? CollectTranslationContext(item)
		: QStringList();
	if (context.isEmpty()) {
		return CreateInteractiveTranslateProvider(
			&peer->session(),
			std::move(error));
	}
	return std::make_unique<ExternalTranslateProvider>(
		service,
		std::move(error),
		QString(),
		std::move(context));
}

} // namespace Nagram
