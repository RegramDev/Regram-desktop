#pragma once

#include "regram/nagram/core/options.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QStringList>
#include <QtCore/QUrlQuery>
#include <QtCore/QUrl>

#include <optional>
#include <utility>
#include <vector>

namespace Nagram {

[[nodiscard]] bool ValidServicesBytes(const QByteArray &raw);
inline const auto kServicesConfig = Option<QByteArray>{
	"nagram.services", Scope::Device, QByteArray(),
	Category::Services, "lng_nagram_services",
	static_cast<unsigned>(Flag::LocalOnly), ValidServicesBytes };
inline constexpr auto kPreferSystemAi = Option<bool>{
	"nagram.preferSystemAi", Scope::Device, false,
	Category::Services, "lng_nagram_system_ai",
	static_cast<unsigned>(Flag::RefreshComposeButtons) };

inline constexpr auto kTranslationContext = Option<bool>{
	"nagram.translationContext", Scope::Device, false,
	Category::Services, "lng_nagram_service_use_context" };
inline constexpr auto kQuickTranslateButton = Option<bool>{
	"nagram.regramQuickTranslateButton", Scope::Device, false,
	Category::Services, "lng_regram_quick_translate",
	static_cast<unsigned>(Flag::RefreshMessageView) };
inline constexpr auto kChatTranslationUseService = Option<bool>{
	"nagram.chatTranslationUseService", Scope::Device, false,
	Category::Services, "lng_nagram_chat_translation_service" };

inline void RegisterServiceOptions(Registry &registry) {
	Expects(registry.Add(kServicesConfig));
	Expects(registry.Add(kPreferSystemAi));
	Expects(registry.Add(kTranslationContext));
	Expects(registry.Add(kQuickTranslateButton));
	Expects(registry.Add(kChatTranslationUseService));
}

enum class ServiceKind {
	Translation,
	Transcription,
};

struct ServiceDefinition {
	QString id;
	QString name;
	ServiceKind kind = ServiceKind::Translation;
	QString protocol;
	QUrl baseUrl;
	QString endpoint;
	QString model;
	QString credentialRef;
	bool useKey = true;
	QString systemPrompt;
	QString prompt;
	QString summaryPrompt;
	QString language;
	std::optional<double> temperature;
};

[[nodiscard]] QJsonObject ServicesDefaults();
[[nodiscard]] QJsonObject UpgradeServices(QJsonObject value);
[[nodiscard]] std::optional<ServiceDefinition> ParseService(const QJsonObject &value);
[[nodiscard]] QJsonObject SerializeService(const ServiceDefinition &value);
[[nodiscard]] bool ValidServices(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> Services();
[[nodiscard]] bool SetServices(const QJsonObject &value);
[[nodiscard]] bool PrefersSystemAi();
[[nodiscard]] std::optional<ServiceDefinition> FindService(
	const QJsonObject &settings,
	const QString &id);
[[nodiscard]] QString CredentialAccount(const ServiceDefinition &service);
[[nodiscard]] QUrl ServiceEndpoint(const ServiceDefinition &service);

struct TranslationCall {
	QJsonDocument body;
	QUrlQuery query;
};

[[nodiscard]] bool TranslationProtocol(const QString &protocol);
[[nodiscard]] bool LlmProtocol(const QString &protocol);
[[nodiscard]] QJsonObject BuildLlmBody(
	const ServiceDefinition &service,
	const QString &system,
	const QString &user);
[[nodiscard]] std::optional<QString> ParseLlmText(
	const ServiceDefinition &service,
	const QByteArray &body);
[[nodiscard]] TranslationCall BuildTranslationCall(
	const ServiceDefinition &service,
	const QStringList &texts,
	const QString &to,
	const QStringList &context = {});
[[nodiscard]] std::optional<QStringList> ParseTranslationResult(
	const ServiceDefinition &service,
	const QByteArray &body,
	int expected);
[[nodiscard]] std::vector<std::pair<QByteArray, QByteArray>> AuthHeaders(
	const ServiceDefinition &service,
	const QByteArray &secret);

} // namespace Nagram
