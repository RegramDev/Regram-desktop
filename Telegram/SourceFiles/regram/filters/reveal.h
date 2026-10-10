#pragma once

class PeerData;

namespace Regram::Filters {

[[nodiscard]] bool TemporarilyRevealed(not_null<PeerData*> peer);
[[nodiscard]] bool ToggleTemporaryReveal(not_null<PeerData*> peer);

} // namespace Regram::Filters
