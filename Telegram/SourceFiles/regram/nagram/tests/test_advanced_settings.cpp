#include "regram/settings/advanced_model.h"
#include "regram/nagram/core/exchange.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <map>
#include <stdexcept>

namespace {

class MemoryPrefs final : public Nagram::RawPrefs {
public:
	QByteArray read(std::string_view key) override;
	void write(std::string_view key, const QByteArray &value) override;
	void clear(std::string_view key) override;

	std::map<std::string, QByteArray> values;

};

QByteArray MemoryPrefs::read(std::string_view key) {
	const auto found = values.find(std::string(key));
	return found == values.end() ? QByteArray() : found->second;
}

void MemoryPrefs::write(std::string_view key, const QByteArray &value) {
	values[std::string(key)] = value;
}

void MemoryPrefs::clear(std::string_view key) {
	values.erase(std::string(key));
}

void Require(bool value, const char *message) {
	if (!value) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestAdvancedSettings() {
	using namespace Regram::Settings;
	auto source = QJsonObject{
		{ u"translationProvider"_q, u"google"_q },
		{ u"saveDeletedMessages"_q, true },
		{ u"useGlobalGhostMode"_q, true },
		{ u"shadowBanIds"_q, QJsonArray{ u"private-user-id"_q } },
		{ u"ghostModeSettings"_q, QJsonObject{
			{ u"0"_q, QJsonObject{ { u"sendReadMessages"_q, false } } },
			{ u"123"_q, QJsonObject{ { u"sendReadMessages"_q, false } } },
		} },
		{ u"messageShotSettings"_q, QJsonObject{
			{ u"showDate"_q, true },
			{ u"cloudThemeAccessHash"_q, u"private-access-hash"_q },
		} },
	};
	const auto raw = PortableAdvancedSettings(source);
	const auto parsed = ParseAdvancedSettings(raw);
	Require(parsed && parsed->value(u"settings"_q).toObject()
		.value(u"translationProvider"_q) == QJsonValue(u"google"_q),
		"advanced translation preference missing from backup");
	Require(!raw.contains("private-") && !raw.contains("123")
		&& parsed->value(u"globalGhost"_q).toObject()
			.value(u"sendReadMessages"_q) == QJsonValue(false)
		&& parsed->value(u"messageShot"_q).toObject()
			.value(u"showDate"_q) == QJsonValue(true),
		"portable settings leak account data or lose global privacy");
	auto registry = Nagram::Registry();
	Require(registry.Add(kAdvancedSettings), "advanced snapshot not registered");
	auto first = MemoryPrefs();
	auto options = Nagram::Options(first);
	Require(options.Set(kAdvancedSettings, raw), "advanced settings rejected");
	const auto exported = Nagram::Exchange::Export(options, registry);
	Require(exported.invalidKeys.empty() && exported.data.contains("translationProvider"),
		"file backup omits advanced settings");
	const auto synced = Nagram::Exchange::Export(options, registry, Nagram::ExchangeTarget::Sync);
	Require(!synced.data.contains("translationProvider"), "local privacy settings enter cloud sync");
	auto second = MemoryPrefs();
	auto destination = Nagram::Options(second);
	const auto plan = Nagram::Exchange::PlanImport(destination, registry, exported.data);
	Require(plan.error.isEmpty() && Nagram::Exchange::Apply(destination, registry, plan).applied
		&& destination.Get(kAdvancedSettings) == raw,
		"advanced settings do not survive export to a fresh profile");
	auto damaged = *parsed;
	damaged.insert(u"version"_q, 2);
	Require(!ParseAdvancedSettings(QJsonDocument(damaged).toJson()),
		"future advanced settings are accepted silently");
	damaged = *parsed;
	damaged.insert(u"globalGhost"_q, QJsonObject{ { u"userId"_q, 123 } });
	Require(!ParseAdvancedSettings(QJsonDocument(damaged).toJson()),
		"account identifiers can be injected through the backup");
	source.insert(u"translationProvider"_q, u"unknown"_q);
	Require(!ValidAdvancedSettings(PortableAdvancedSettings(source)),
		"unknown provider accepted through advanced settings backup");
	source.insert(u"translationProvider"_q, u"telegram"_q);
	source.insert(u"wideMultiplier"_q, 4.01);
	Require(!ValidAdvancedSettings(PortableAdvancedSettings(source)),
		"out-of-range appearance value accepted");
	std::cout << "PASS: Advanced settings backup, isolation and recovery" << std::endl;
}
