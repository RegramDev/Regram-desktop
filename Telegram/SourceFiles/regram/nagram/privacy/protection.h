#pragma once

#include <gsl/pointers>
#include <rpl/producer.h>

class HistoryItem;
class PeerData;
namespace Main {
class Session;
} // namespace Main
namespace Window {
class SessionController;
} // namespace Window

namespace Nagram::Privacy {

[[nodiscard]] bool CopyAllowed(bool force, bool allowsForwarding);
[[nodiscard]] bool CopyForbidden(bool force, bool forbidsForward);
[[nodiscard]] bool SensitiveWarningSkipped(
	bool enabled,
	bool loaded,
	bool canChange,
	bool ageVerifyNeeded);

[[nodiscard]] bool DoNotSharePhoneByDefault();

[[nodiscard]] bool ForceCopy();
[[nodiscard]] rpl::producer<bool> ForceCopyChanges();
[[nodiscard]] bool AllowsCopy(gsl::not_null<const PeerData*> peer);
[[nodiscard]] bool ForbidsCopy(gsl::not_null<const HistoryItem*> item);
[[nodiscard]] rpl::producer<bool> AllowsCopyValue(
	gsl::not_null<PeerData*> peer);

[[nodiscard]] bool IgnoreRestrictions();
void WatchRestrictions(gsl::not_null<Window::SessionController*> controller);

[[nodiscard]] bool SkipSensitiveWarning(gsl::not_null<Main::Session*> session);
void AttachSensitive(gsl::not_null<Main::Session*> session);
[[nodiscard]] rpl::producer<> SensitiveRevealed(
	gsl::not_null<Main::Session*> session);

} // namespace Nagram::Privacy
