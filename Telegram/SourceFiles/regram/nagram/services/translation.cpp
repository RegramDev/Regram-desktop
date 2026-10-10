#include "regram/nagram/services/translation.h"

#include "base/weak_ptr.h"
#include "boxes/translate_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/core_settings.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "iv/iv_rich_page.h"
#include "lang/lang_keys.h"
#include "lang/translate_mtproto_provider.h"
#include "lang/translate_provider.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "platform/platform_translate_provider.h"
#include "regram/nagram/services/context.h"
#include "regram/nagram/services/request.h"
#include "ui/layers/generic_box.h"
#include "window/window_session_controller.h"

#include <QtCore/QCache>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <map>

namespace Nagram {
namespace {

struct QuickTranslation {
	std::unique_ptr<Ui::TranslateProvider> provider;
	std::unique_ptr<MTP::Sender> api;
	std::shared_ptr<const Iv::RichPage> originalPage;
	std::shared_ptr<Iv::RichPage> translatedPage;
	std::vector<TextWithEntities*> fields;
	size_t nextField = 0;
	bool pending = false;
};

struct TranslationEligibility {
	FullMsgId id;
	QString text;
	bool eligible = false;
};

struct EligibilityCache {
	base::weak_ptr<Main::Session> session;
	QCache<quintptr, TranslationEligibility> entries;
	rpl::lifetime lifetime;
	uint64 generation = 0;
};

EligibilityCache &EligibilityFor(not_null<Main::Session*> session) {
	static auto caches = std::map<Main::Session*, std::unique_ptr<EligibilityCache>>();
	auto found = caches.find(session);
	if (found == caches.end() || !found->second->session) {
		std::erase_if(caches, [](const auto &entry) {
			return !entry.second->session;
		});
		auto cache = std::make_unique<EligibilityCache>();
		cache->session = base::make_weak(session);
		cache->entries.setMaxCost(4 * 1024 * 1024);
		const auto raw = cache.get();
		Core::App().settings().skipTranslationLanguagesValue()
		| rpl::skip(1) | rpl::on_next([raw] {
			++raw->generation;
			auto ids = std::vector<FullMsgId>();
			for (const auto key : raw->entries.keys()) {
				ids.push_back(raw->entries.object(key)->id);
			}
			raw->entries.clear();
			if (const auto session = raw->session.get()) {
				for (const auto id : ids) {
					if (const auto item = session->data().message(id)) {
						session->data().requestItemViewRefresh(item);
					}
				}
			}
		}, cache->lifetime);
		found = caches.emplace(session, std::move(cache)).first;
	}
	return *found->second;
}

void ApplyQuickTranslation(
		not_null<Main::Session*> session,
		FullMsgId id,
		const TextWithEntities &original,
		LanguageId to,
		Ui::TranslateProviderResult result,
		not_null<Window::SessionController*> controller) {
	const auto item = session->data().message(id);
	if (!item || item->originalText() != original) {
		return;
	}
	if (result.error != Ui::TranslateProviderError::None
		|| !result.text || result.text->empty()) {
		controller->showToast(result.error
			== Ui::TranslateProviderError::LocalLanguagePackMissing
			? tr::lng_translate_box_error_language_pack_not_installed(tr::now)
			: tr::lng_translate_box_error(tr::now));
		return;
	}
	const auto requested = item->translationShowRequiresRequest(to);
	if (requested || item->translation()->text.empty()) {
		item->translationDone(to, std::move(*result.text));
	}
	const auto retry = item->translationShowRequiresRequest(to);
	Assert(!retry);
	item->translationSetManual(true);
}

void FinishRichTranslation(
		const std::weak_ptr<QuickTranslation> &weakState,
		base::weak_ptr<Main::Session> session,
		FullMsgId id,
		const TextWithEntities &original,
		LanguageId to,
		not_null<Window::SessionController*> controller,
		const MTPmessages_TranslatedRichMessage &result) {
	const auto state = weakState.lock();
	const auto owner = session.get();
	if (!state || !owner) {
		return;
	}
	state->pending = false;
	const auto &list = result.data().vresult().v;
	const auto page = list.isEmpty() ? nullptr
		: Iv::ParseRichPage(owner, list.front());
	const auto item = owner->data().message(id);
	if (!item || item->originalText() != original || item->richPage() != state->originalPage) {
		return;
	} else if (!page) {
		controller->showToast(tr::lng_translate_box_error(tr::now));
		return;
	}
	const auto requested = item->translationShowRequiresRequest(to);
	if (requested || item->translation()->text.empty()) {
		item->translationDone(to, page);
	}
	const auto retry = item->translationShowRequiresRequest(to);
	Assert(!retry);
	item->translationSetManual(true);
}

void SendRichTranslation(
		const std::shared_ptr<QuickTranslation> &state,
		base::weak_ptr<Main::Session> session,
		FullMsgId id,
		TextWithEntities original,
		LanguageId to,
		not_null<Window::SessionController*> controller) {
	const auto owner = session.get();
	const auto item = owner->data().message(id);
	const auto weakState = std::weak_ptr<QuickTranslation>(state);
	state->api = std::make_unique<MTP::Sender>(&owner->mtp());
	using Flag = MTPmessages_TranslateRichMessage::Flag;
	state->api->request(MTPmessages_TranslateRichMessage(
		MTP_flags(Flag::f_peer | Flag::f_id),
		item->history()->peer->input(),
		MTP_vector<MTPint>(1, MTP_int(item->id)),
		MTPVector<MTPInputRichMessage>(),
		MTP_string(to.twoLetterCode()),
		MTPstring()
	)).done(crl::guard(controller, [=](
			const MTPmessages_TranslatedRichMessage &result) {
		FinishRichTranslation(weakState, session, id, original, to, controller, result);
	})).fail(crl::guard(controller, [=](const MTP::Error &error) {
		if (const auto state = weakState.lock()) {
			state->pending = false;
			if (!MTP::IgnoreError(error)) {
				controller->showToast(tr::lng_translate_box_error(tr::now));
			}
		}
	})).send();
}

bool CollectRichTranslationFields(
		std::vector<Iv::RichPage::Block> &blocks,
		std::vector<TextWithEntities*> &fields,
		int depth = 0) {
	if (depth > 32 || fields.size() > 2000) {
		return false;
	}
	const auto add = [&](Iv::RichPage::RichText &text) {
		if (!text.text.empty()) {
			fields.push_back(&text.text);
		}
	};
	for (auto &block : blocks) {
		if (block.kind != Iv::RichPage::BlockKind::Code
			&& block.kind != Iv::RichPage::BlockKind::Math) {
			add(block.text);
		}
		add(block.caption);
		for (auto &item : block.listItems) {
			add(item.text);
			if (!CollectRichTranslationFields(item.blocks, fields, depth + 1)) {
				return false;
			}
		}
		for (auto &row : block.tableRows) {
			for (auto &cell : row.cells) {
				add(cell.text);
			}
		}
		for (auto &button : block.buttons) {
			add(button.text);
		}
		if (!CollectRichTranslationFields(block.blocks, fields, depth + 1)) {
			return false;
		}
	}
	return fields.size() <= 2000;
}

void TranslateRichField(
		const std::shared_ptr<QuickTranslation> &state,
		base::weak_ptr<Main::Session> session,
		FullMsgId id,
		LanguageId to,
		not_null<Window::SessionController*> controller) {
	const auto owner = session.get();
	const auto item = owner ? owner->data().message(id) : nullptr;
	if (!item || item->richPage() != state->originalPage) {
		state->pending = false;
		return;
	}
	if (state->nextField == state->fields.size()) {
		const auto requested = item->translationShowRequiresRequest(to);
		if (requested || item->translation()->text.empty()) {
			item->translationDone(to, state->translatedPage);
		}
		const auto retry = item->translationShowRequiresRequest(to);
		Assert(!retry);
		item->translationSetManual(true);
		state->pending = false;
		state->fields.clear();
		state->originalPage.reset();
		state->translatedPage.reset();
		return;
	}
	const auto weak = std::weak_ptr<QuickTranslation>(state);
	state->provider->request({ .text = *state->fields[state->nextField] }, to,
		crl::guard(controller, [=](Ui::TranslateProviderResult result) {
			const auto state = weak.lock();
			if (!state) {
				return;
			}
			if (result.error != Ui::TranslateProviderError::None || !result.text) {
				state->pending = false;
				controller->showToast(tr::lng_translate_box_error(tr::now));
				return;
			}
			*state->fields[state->nextField++] = std::move(*result.text);
			crl::on_main(controller, [=] {
				TranslateRichField(state, session, id, to, controller);
			});
		}));
}

void ToggleQuickTranslation(
		const std::shared_ptr<QuickTranslation> &state,
		base::weak_ptr<Main::Session> session,
		FullMsgId id,
		const ClickContext &context) {
	const auto weakState = std::weak_ptr<QuickTranslation>(state);
	const auto owner = session.get();
	const auto controller = context.other.value<ClickHandlerContext>().sessionWindow.get();
	if (!owner || !controller || &controller->session() != owner) {
		return;
	}
	const auto item = owner->data().message(id);
	if (!item || state->pending) {
		return;
	}
	const auto to = Ui::ChooseTranslateTo(item->history());
	const auto translation = item->translation();
	if (translation && translation->used
		&& (translation->manual || translation->to == item->history()->translatedTo())) {
		const auto requested = item->translationShowRequiresRequest({});
		Assert(!requested);
		item->translationSetManual(false);
		return;
	} else if (translation && translation->to == to
		&& !translation->text.empty()) {
		const auto requested = item->translationShowRequiresRequest(to);
		Assert(!requested);
		item->translationSetManual(true);
		return;
	}
	const auto original = item->originalText();
	state->provider = CreateMessageTranslateProvider(
		item->history()->peer,
		item->id,
		item->forbidsForward(),
		crl::guard(controller, [=](QString error) {
			controller->showToast(error);
		}));
	state->pending = true;
	if (item->richPage() && state->provider->supportsMessageId()
		&& IsServerMsgId(item->id)) {
		state->originalPage = item->richPage();
		SendRichTranslation(state, session, id, original, to, controller);
		return;
	} else if (item->richPage()) {
		state->originalPage = item->richPage();
		state->translatedPage = std::make_shared<Iv::RichPage>(*state->originalPage);
		state->fields.clear();
		state->nextField = 0;
		if (!CollectRichTranslationFields(state->translatedPage->blocks, state->fields)) {
			state->pending = false;
			controller->showToast(tr::lng_translate_box_error(tr::now));
			return;
		}
		TranslateRichField(state, session, id, to, controller);
		return;
	}
	state->provider->request(
		Ui::PrepareTranslateProviderRequest(
			state->provider.get(), item->history()->peer, item->id, original),
		to,
		crl::guard(controller, [=](Ui::TranslateProviderResult result) {
			if (const auto state = weakState.lock()) {
				state->pending = false;
				if (const auto owner = session.get()) {
					ApplyQuickTranslation(owner, id, original, to,
						std::move(result), controller);
				}
			}
		}));
}

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

bool CanQuickTranslate(not_null<HistoryItem*> item) {
	const auto &text = item->originalText();
	auto &cache = EligibilityFor(&item->history()->session());
	const auto key = reinterpret_cast<quintptr>(item.get());
	const auto cached = cache.entries.object(key);
	if (cached && cached->id == item->fullId() && cached->text == text.text) {
		return cached->eligible;
	}
	const auto session = cache.session;
	const auto generation = cache.generation;
	const auto id = item->fullId();
	const auto source = text.text;
	const auto skip = Core::App().settings().skipTranslationLanguages();
	cache.entries.insert(key, new TranslationEligibility{
		.id = id,
		.text = source,
	}, std::max(1, int(text.text.size() * sizeof(QChar) + sizeof(TranslationEligibility))));
	crl::async([=] {
		const auto eligible = !Ui::SkipTranslationLanguage({ source }, skip);
		crl::on_main([=] {
			const auto owner = session.get();
			if (!owner) {
				return;
			}
			auto &cache = EligibilityFor(owner);
			const auto cached = cache.entries.object(key);
			if (!cached || cache.generation != generation
				|| cached->id != id || cached->text != source) {
				return;
			}
			cached->eligible = eligible;
			if (const auto item = owner->data().message(id)) {
				owner->data().requestItemViewRefresh(item);
				owner->data().requestItemResize(item);
			}
		});
	});
	return false;
}

ClickHandlerPtr QuickTranslateClickHandler(not_null<HistoryItem*> item) {
	const auto session = base::make_weak(&item->history()->session());
	const auto id = item->fullId();
	const auto state = std::make_shared<QuickTranslation>();
	return std::make_shared<LambdaClickHandler>([=](ClickContext context) {
		ToggleQuickTranslation(state, session, id, context);
	});
}

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
