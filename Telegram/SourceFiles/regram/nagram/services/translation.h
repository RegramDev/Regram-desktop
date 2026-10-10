#pragma once

#include "regram/nagram/services/translation_model.h"
#include "lang/translate_provider.h"

namespace Main {
class Session;
} // namespace Main

class PeerData;

namespace Nagram {

struct ServiceDefinition;

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
