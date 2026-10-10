#include "regram/nagram/core/diagnostics.h"

#include "core/application.h"
#include "lang/lang_keys.h"
#include "mtproto/mtp_instance.h"
#include "window/window_controller.h"

#include <QtCore/QHash>

namespace Nagram {
namespace {

constexpr auto kRepeatDelay = 5 * crl::time(1000);

} // namespace

void InstallRpcErrorObserver() {
	static auto installed = false;
	if (installed) {
		return;
	}
	installed = true;
	MTP::SetRpcErrorObserver([](const MTP::Error &error) {
		if (!ForDevice().Get(kShowRpcErrors)) {
			return;
		}
		static auto shown = QHash<QString, crl::time>();
		const auto now = crl::now();
		const auto key = error.type();
		if (const auto i = shown.find(key)
			; i != shown.end() && now - i.value() < kRepeatDelay) {
			return;
		}
		shown.insert(key, now);
		if (const auto window = Core::App().activeWindow()) {
			window->showToast(TextWithEntities{ tr::lng_nagram_rpc_error(
				tr::now,
				lt_code,
				QString::number(error.code()),
				lt_error,
				error.type()) });
		}
	});
}

} // namespace Nagram
