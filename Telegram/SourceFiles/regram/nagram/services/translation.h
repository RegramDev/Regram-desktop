#pragma once

#include "base/weak_ptr.h"
#include "lang/translate_provider.h"
#include "regram/nagram/services/translation_model.h"

#include <memory>

namespace Iv {
struct RichPage;
} // namespace Iv

namespace Main {
class Session;
} // namespace Main
namespace Window {
class SessionController;
} // namespace Window

class PeerData;
class HistoryItem;

namespace Nagram {

struct ServiceDefinition;

class RichPageTranslation final : public base::has_weak_ptr {
public:
	using Done = Fn<void(std::shared_ptr<const Iv::RichPage>)>;

	RichPageTranslation(
		not_null<Main::Session*> session,
		std::shared_ptr<const Iv::RichPage> original,
		LanguageId to,
		Done done);

private:
	void translateNext();
	void finish(std::shared_ptr<const Iv::RichPage> result);

	base::weak_ptr<Main::Session> _session;
	std::shared_ptr<Iv::RichPage> _page;
	std::vector<TextWithEntities*> _fields;
	std::unique_ptr<Ui::TranslateProvider> _provider;
	LanguageId _to;
	Done _done;
	size_t _next = 0;

};

[[nodiscard]] bool UsesTelegramTranslation();
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> PreserveTranslationEntities(
	std::unique_ptr<Ui::TranslateProvider> provider);

[[nodiscard]] ClickHandlerPtr QuickTranslateClickHandler(
	not_null<HistoryItem*> item);
[[nodiscard]] bool CanQuickTranslate(not_null<HistoryItem*> item);
void ToggleMessageTranslation(
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> CreateServiceTranslateProvider(
	const ServiceDefinition &service,
	Fn<void(QString)> error);
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> CreateInteractiveTranslateProvider(
	not_null<Main::Session*> session,
	Fn<void(QString)> error);
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> CreateMessageTranslateProvider(
	not_null<PeerData*> peer,
	MsgId msgId,
	bool hasCopyRestriction,
	Fn<void(QString)> error);

} // namespace Nagram
