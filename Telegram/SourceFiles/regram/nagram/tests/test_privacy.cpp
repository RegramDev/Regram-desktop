#include "regram/nagram/core/exchange.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/privacy/protection.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

class MemoryPrefs final : public Nagram::RawPrefs {
public:
	[[nodiscard]] QByteArray read(std::string_view key) override {
		const auto found = values.find(std::string(key));
		return (found == values.end()) ? QByteArray() : found->second;
	}
	void write(std::string_view key, const QByteArray &value) override {
		values[std::string(key)] = value;
	}
	void clear(std::string_view key) override {
		values.erase(std::string(key));
	}

	std::map<std::string, QByteArray> values;
};

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

void CheckSwitch(
		const Nagram::Registry &registry,
		const Nagram::Option<bool> &option,
		bool refreshesMessages) {
	using namespace Nagram;
	const auto info = registry.Find(option.key);
	Require(info != nullptr, "protection option is not registered");
	Require(info->type == OptionInfo::ValueType::Boolean
		&& info->fallbackRaw == "0"
		&& !option.fallback, "protection option must default to off");
	Require(info->scope == Scope::Device
		&& info->category == Category::Privacy
		&& registry.HasFlag(option.key, Flag::Exportable),
		"protection option must be an exportable device option");
	Require(registry.HasFlag(option.key, Flag::RefreshMessageView)
		== refreshesMessages, "protection option refresh flag");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Get(option), "protection option default value");
	Require(options.Set(option, true), "protection option write");
	const auto exported = Exchange::Export(options, registry);
	Require(exported.invalidKeys.empty(), "protection option export");
	const auto key = QString::fromUtf8(option.key.data(), option.key.size());
	Require(QJsonDocument::fromJson(exported.data).object().value(
		QString::fromLatin1("options")).toObject().value(key)
			== QJsonValue(true), "protection option missing from export");

	auto importedPrefs = MemoryPrefs();
	auto imported = Options(importedPrefs);
	const auto plan = Exchange::PlanImport(imported, registry, exported.data);
	Require(plan.error.isEmpty() && plan.changes.size() == 1,
		"protection option import preview");
	Require(Exchange::Apply(imported, registry, plan).applied
		&& imported.Get(option), "protection option import");

	importedPrefs.values[std::string(option.key)] = "broken";
	Require(!imported.Get(option)
		&& imported.invalidKeys().contains(option.key)
		&& importedPrefs.values[std::string(option.key)] == "broken",
		"invalid protection value must fall back and stay stored");
}

void CheckCopyRules() {
	using namespace Nagram::Privacy;
	for (auto mask = 0; mask != 32; ++mask) {
		const auto force = (mask & 1) != 0;
		const auto peerAllows = (mask & 2) != 0;
		const auto itemForbids = (mask & 4) != 0;
		const auto expiring = (mask & 8) != 0;
		const auto paid = (mask & 16) != 0;
		const auto copyRestricted = !CopyAllowed(force, peerAllows)
			|| CopyForbidden(force, itemForbids);
		const auto savingForbidden = CopyForbidden(force, itemForbids)
			|| expiring
			|| paid;
		const auto mediaRestricted = copyRestricted || savingForbidden;
		const auto downloadControls = !savingForbidden
			&& CopyAllowed(force, peerAllows);
		if (force) {
			Require(!copyRestricted, "forced copy still restricts text");
			Require(mediaRestricted == (expiring || paid),
				"forced copy must keep expiring and paid media protected");
			Require(downloadControls == (!expiring && !paid),
				"forced copy download controls");
		} else {
			const auto upstreamCopy = !peerAllows || itemForbids;
			const auto upstreamSaving = itemForbids || expiring || paid;
			Require(CopyAllowed(force, peerAllows) == peerAllows
				&& CopyForbidden(force, itemForbids) == itemForbids,
				"copy rules differ from upstream when switched off");
			Require(copyRestricted == upstreamCopy
				&& savingForbidden == upstreamSaving
				&& mediaRestricted == (upstreamCopy || upstreamSaving)
				&& downloadControls == (!upstreamSaving && peerAllows),
				"restrictions differ from upstream when switched off");
		}
	}
}

void CheckSensitiveRules() {
	using namespace Nagram::Privacy;
	for (auto mask = 0; mask != 16; ++mask) {
		const auto enabled = (mask & 1) != 0;
		const auto loaded = (mask & 2) != 0;
		const auto canChange = (mask & 4) != 0;
		const auto ageVerifyNeeded = (mask & 8) != 0;
		Require(SensitiveWarningSkipped(
			enabled,
			loaded,
			canChange,
			ageVerifyNeeded) == (mask == 7),
			"sensitive media shown without an adjustable account setting");
	}
}

} // namespace

void TestPrivacy() {
	using namespace Nagram;
	auto registry = Registry();
	Privacy::RegisterOptions(registry);
	CheckSwitch(registry, Privacy::kDoNotSharePhone, false);
	CheckSwitch(registry, Privacy::kForceCopy, true);
	CheckSwitch(registry, Privacy::kIgnoreContentRestrictions, true);
	CheckSwitch(registry, Privacy::kSkipSensitiveWarning, true);
	CheckSensitiveRules();
	CheckCopyRules();

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Get(Privacy::kDoNotSharePhone),
		"phone number sharing must stay ticked by default");
	Require(options.Set(Privacy::kDoNotSharePhone, true)
		&& options.Get(Privacy::kDoNotSharePhone),
		"phone number sharing must start unticked when enabled");
	std::cout << "PASS: Nagram content protection" << std::endl;
}
