#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Regram::Diagnostics {

struct TransferSnapshot {
	uint64 receivedBytes = 0;
	uint64 bytesPerSecond = 0;
};

void RecordMediaReceived(not_null<Main::Session*> session, int64 bytes);
[[nodiscard]] TransferSnapshot Transfers(not_null<Main::Session*> session);
void SettingsBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session);

} // namespace Regram::Diagnostics
