#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram {

void BackupToCloud(not_null<Window::SessionController*> controller);
void RestoreFromCloud(not_null<Window::SessionController*> controller);
void SyncWithCloud(not_null<Window::SessionController*> controller);
void DeleteCloudBackup(not_null<Window::SessionController*> controller);
[[nodiscard]] rpl::producer<QString> CloudSyncStatus(
	not_null<Main::Session*> session);
[[nodiscard]] rpl::producer<bool> CloudSyncAutoValue(
	not_null<Main::Session*> session);
void SetCloudSyncAuto(not_null<Main::Session*> session, bool enabled);
void AttachCloudSync(not_null<Main::Session*> session);

} // namespace Nagram
