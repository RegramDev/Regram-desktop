#include "regram/nagram/core/exchange.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/privacy/registration_model.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <cstdlib>
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

template <typename Type>
void CheckDeviceOption(
		const Nagram::Registry &registry,
		const Nagram::Option<Type> &option,
		Nagram::Category category,
		const Type &changed) {
	using namespace Nagram;
	const auto info = registry.Find(option.key);
	Require(info != nullptr, "P3-09 option is not registered");
	Require(info->scope == Scope::Device
		&& info->category == category
		&& registry.HasFlag(option.key, Flag::Exportable),
		"P3-09 option must be an exportable device option");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(options.Get(option) == option.fallback
		&& prefs.values.empty(), "P3-09 option default value");
	Require(options.Set(option, changed)
		&& options.Get(option) == changed, "P3-09 option write");
	const auto exported = Exchange::Export(options, registry);
	Require(exported.invalidKeys.empty(), "P3-09 option export");

	auto importedPrefs = MemoryPrefs();
	auto imported = Options(importedPrefs);
	const auto plan = Exchange::PlanImport(imported, registry, exported.data);
	Require(plan.error.isEmpty() && plan.changes.size() == 1,
		"P3-09 option import preview");
	Require(Exchange::Apply(imported, registry, plan).applied
		&& imported.Get(option) == changed, "P3-09 option import");

	importedPrefs.values[std::string(option.key)] = "broken";
	Require(imported.Get(option) == option.fallback
		&& imported.invalidKeys().contains(option.key)
		&& importedPrefs.values[std::string(option.key)] == "broken",
		"invalid P3-09 value must fall back and stay stored");
}

void TestLinkBehavior() {
	using namespace Nagram;
	using namespace Nagram::Links;
	auto registry = Registry();
	RegisterBehaviorOptions(registry);
	CheckDeviceOption(registry, kDisableOfficialAutoLogin,
		Category::Rules, true);
	CheckDeviceOption(registry, kHashtagSearchPageChannel,
		Category::Rules, 2);
	CheckDeviceOption(registry, kHashtagSearchPageChat,
		Category::Rules, 1);
	for (const auto option : {
			&kHashtagSearchPageChannel,
			&kHashtagSearchPageChat }) {
		Require(option->fallback == 0
			&& option->validate(0)
			&& option->validate(1)
			&& option->validate(2)
			&& !option->validate(3)
			&& !option->validate(-1), "hashtag search page bounds");
	}

	using Page = HashtagPage;
	for (const auto broadcast : { false, true }) {
		Require(ResolveHashtagPage(true, broadcast, 0, 0) == Page::Follow,
			"default hashtag page must follow Telegram");
		for (auto channel = 0; channel != 3; ++channel) {
			for (auto chat = 0; chat != 3; ++chat) {
				Require(ResolveHashtagPage(false, broadcast, channel, chat)
					== Page::Follow,
					"hashtag page changed outside of a hashtag click");
			}
		}
	}
	Require(ResolveHashtagPage(true, true, 1, 2) == Page::ThisChat
		&& ResolveHashtagPage(true, true, 2, 1) == Page::MyMessages
		&& ResolveHashtagPage(true, true, 0, 2) == Page::Follow,
		"channels must use the channel hashtag page");
	Require(ResolveHashtagPage(true, false, 1, 2) == Page::MyMessages
		&& ResolveHashtagPage(true, false, 2, 1) == Page::ThisChat
		&& ResolveHashtagPage(true, false, 2, 0) == Page::Follow,
		"other chats must use the chat hashtag page");

	auto prefs = MemoryPrefs();
	auto options = Options(prefs);
	Require(!options.Get(kDisableOfficialAutoLogin),
		"official auto-login must stay enabled by default");
	Require(options.Set(kDisableOfficialAutoLogin, true)
		&& options.Get(kDisableOfficialAutoLogin),
		"official auto-login switch");
}

void TestWebAppSize() {
	using namespace Nagram;
	using namespace Nagram::Links;
	auto registry = Registry();
	RegisterBehaviorOptions(registry);
	CheckDeviceOption(registry, kWebAppWidthScale, Category::Rules, 150);
	CheckDeviceOption(registry, kWebAppHeightScale, Category::Rules, 200);
	for (const auto option : { &kWebAppWidthScale, &kWebAppHeightScale }) {
		Require(option->fallback == 100
			&& option->validate(100)
			&& option->validate(125)
			&& option->validate(200)
			&& !option->validate(75)
			&& !option->validate(110)
			&& !option->validate(225), "web app scale bounds");
	}

	const auto base = QSize(384, 694);
	const auto screen = QSize(1440, 860);
	Require(ScaledPanelSize(base, 100, 100, screen) == base
		&& ScaledPanelSize(base, 100, 100, QSize()) == base
		&& ScaledPanelSize(base, 100, 100, QSize(300, 500)) == base,
		"default web app size must stay the upstream size");
	Require(ScaledPanelSize(base, 150, 100, screen) == QSize(576, 694)
		&& ScaledPanelSize(base, 200, 125, screen) == QSize(768, 860),
		"web app size must scale and stay inside the available area");
	Require(ScaledPanelSize(base, 200, 200, QSize()) == QSize(768, 1388),
		"web app size without a screen must return the scaled size");
	Require(ScaledPanelSize(base, 200, 200, QSize(300, 500)) == base,
		"web app size must not shrink below the upstream size");
}

void TestRegistrationDate() {
	using namespace Nagram;
	using namespace Nagram::Privacy;
	auto registry = Registry();
	RegisterOptions(registry);
	CheckDeviceOption(registry, kShowRegistrationDate,
		Category::Privacy, true);
	Require(!kShowRegistrationDate.fallback,
		"registration date must be hidden by default");
	Require(!ShowsRegistration(false, true, 5, 2021),
		"registration date shown while the option is off");
	Require(ShowsRegistration(true, true, 1, 2013)
		&& ShowsRegistration(true, true, 12, 2026),
		"registration date sent by Telegram must be shown");
	Require(!ShowsRegistration(true, false, 5, 2021),
		"registration date shown for a group or a channel");
	Require(!ShowsRegistration(true, true, 0, 0)
		&& !ShowsRegistration(true, true, 0, 2021)
		&& !ShowsRegistration(true, true, 13, 2021)
		&& !ShowsRegistration(true, true, 5, 0),
		"registration date shown without data from Telegram");
}

void TestRegistrationEstimate() {
	using namespace Nagram::Privacy;
	using Bound = RegistrationBound;
	const auto anchors = RegistrationAnchors();
	Require(anchors.size() == 133, "registration anchors count");
	for (auto i = 1; i != int(anchors.size()); ++i) {
		Require(anchors[i - 1].id < anchors[i].id,
			"registration anchor IDs must strictly increase");
		Require(anchors[i - 1].date <= anchors[i].date,
			"registration anchor dates must not decrease");
	}
	const auto &first = anchors.front();
	const auto &last = anchors.back();
	using Estimate = RegistrationEstimate;
	Require(EstimateRegistration(first.id - 1)
			== Estimate{ Bound::Before, first.date }
		&& EstimateRegistration(0) == Estimate{ Bound::Before, first.date },
		"ID below the first anchor must be before its date");
	Require(EstimateRegistration(first.id)
			== Estimate{ Bound::About, first.date }
		&& EstimateRegistration(last.id)
			== Estimate{ Bound::About, last.date },
		"anchor IDs must get the anchor dates");
	Require(EstimateRegistration(last.id + 1)
			== Estimate{ Bound::After, last.date },
		"ID above the last anchor must be after its date");
	const auto &second = anchors[1];
	const auto middle = EstimateRegistration((first.id + second.id) / 2);
	Require(middle.bound == Bound::About
		&& middle.date > first.date
		&& middle.date < second.date
		&& std::abs(middle.date - (first.date + second.date) / 2) <= 1,
		"ID between two anchors must be interpolated");
	auto previous = first.date;
	for (auto id = first.id; id <= last.id; id += 7'654'321) {
		const auto date = EstimateRegistration(id).date;
		Require(date >= previous,
			"a larger ID must not get an earlier registration date");
		previous = date;
	}
}

} // namespace

void TestP3Misc() {
	TestLinkBehavior();
	TestWebAppSize();
	TestRegistrationDate();
	TestRegistrationEstimate();
	std::cout << "PASS: Nagram P3-09 options" << std::endl;
}
