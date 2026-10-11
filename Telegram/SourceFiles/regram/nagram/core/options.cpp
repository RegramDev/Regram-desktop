#include "regram/nagram/core/options.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "main/main_session.h"
#include "regram/filters/runtime.h"
#include "regram/nagram/chats/local_pins_model.h"
#include "regram/nagram/chats/options.h"
#include "regram/nagram/compose/options.h"
#include "regram/nagram/core/device_options.h"
#include "regram/nagram/core/diagnostics.h"
#include "regram/nagram/interface/options.h"
#include "regram/nagram/links/inline_rules.h"
#include "regram/nagram/links/model.h"
#include "regram/nagram/links/options.h"
#include "regram/nagram/links/webview.h"
#include "regram/nagram/media/backend_options.h"
#include "regram/nagram/media/local_faved_model.h"
#include "regram/nagram/media/options.h"
#include "regram/nagram/menu/model.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/network/options.h"
#include "regram/nagram/notifications/model.h"
#include "regram/nagram/privacy/alias.h"
#include "regram/nagram/privacy/options.h"
#include "regram/nagram/services/auto_translate_model.h"
#include "regram/nagram/services/model.h"
#include "regram/nagram/services/send_translation_model.h"
#include "regram/nagram/snapshot/snapshot.h"
#include "regram/nagram/sync/model.h"
#include "regram/settings/advanced_model.h"
#include "regram/settings/ios_backup.h"
#include "storage/storage_account.h"

#include <map>
#include <memory>

namespace Storage {

template <>
std::optional<QByteArray> Account::readPrefImpl<QByteArray>(
		std::string_view key) {
	return readPrefGeneric(key);
}

template <>
void Account::writePrefImpl<QByteArray>(
		std::string_view key,
		QByteArray value) {
	writePrefGeneric(key, value);
}

} // namespace Storage

namespace Nagram {

Options &ForDevice() {
	static auto prefs = DevicePrefs(Core::App().settings());
	return details::SharedDeviceOptions(prefs);
}

DevicePrefs::DevicePrefs(Core::Settings &settings) : _settings(settings) { }

QByteArray DevicePrefs::read(std::string_view key) {
	return _settings.readPref<QByteArray>(key);
}

void DevicePrefs::write(std::string_view key, const QByteArray &value) {
	_settings.writePref<QByteArray>(key, value);
}

void DevicePrefs::clear(std::string_view key) {
	_settings.clearPref(key);
}

AccountPrefs::AccountPrefs(Storage::Account &account) : _account(account) { }

QByteArray AccountPrefs::read(std::string_view key) {
	return _account.readPref<QByteArray>(key);
}

void AccountPrefs::write(std::string_view key, const QByteArray &value) {
	_account.writePref<QByteArray>(key, value);
}

void AccountPrefs::clear(std::string_view key) {
	_account.clearPref(key);
}

Options &ForAccount(gsl::not_null<Main::Session*> session) {
	struct State {
		explicit State(gsl::not_null<Main::Session*> session)
		: guard(base::make_weak(session))
		, prefs(session->local())
		, options(prefs, Scope::Account) { }
		base::weak_ptr<Main::Session> guard;
		AccountPrefs prefs;
		Options options;
	};
	// Called while the session is still being constructed, so its lifetime
	// can't own the state: the weak pointer tells when the session is gone.
	static auto states = std::map<Main::Session*, std::unique_ptr<State>>();
	const auto found = states.find(session);
	if (found != states.end() && found->second->guard) {
		return found->second->options;
	}
	std::erase_if(states, [](const auto &entry) {
		return !entry.second->guard;
	});
	const auto inserted = states.emplace(
		session, std::make_unique<State>(session)).first;
	return inserted->second->options;
}

const Registry &RegisteredOptions() {
	static const auto registry = [] {
		auto result = Registry();
		Chats::RegisterOptions(result);
		Chats::RegisterLocalPinOptions(result);
		Interface::RegisterOptions(result);
		Compose::RegisterOptions(result);
		Media::RegisterOptions(result);
		Media::RegisterLocalFavedOptions(result);
		Media::RegisterBackendOptions(result);
		Menu::RegisterOptions(result);
		Privacy::RegisterOptions(result);
		Privacy::RegisterAliasOptions(result);
		Messages::RegisterOptions(result);
		Expects(result.Add(Regram::Filters::kPortableRules));
		Expects(result.Add(Regram::Filters::kSuppressNotifications));
		Expects(result.Add(Regram::Filters::kDisabledChats));
		Expects(result.Add(Regram::Filters::kHiddenSenders));
		Expects(result.Add(Regram::Settings::IosBackup::kSource));
		Links::RegisterOptions(result);
		Links::RegisterBehaviorOptions(result);
		Links::RegisterInlineOptions(result);
		Links::RegisterWebviewOptions(result);
		Network::RegisterOptions(result);
		Notifications::RegisterOptions(result);
		Snapshot::RegisterOptions(result);
		RegisterServiceOptions(result);
		AutoTranslate::RegisterOptions(result);
		SendTranslation::RegisterOptions(result);
		RegisterDiagnosticsOptions(result);
		Expects(result.Add(Regram::Settings::kAdvancedSettings));
		Sync::RegisterOptions(result);
		return result;
	}();
	return registry;
}

} // namespace Nagram
