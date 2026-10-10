#pragma once

#include "base/basic_types.h"
#include "regram/nagram/core/options.h"

class PeerData;
namespace Main { class SessionShow; }

namespace Nagram::Privacy {

using PeerAliases = base::flat_map<PeerId, QString>;

[[nodiscard]] bool ValidAlias(const QString &value);
[[nodiscard]] std::optional<PeerAliases> ParseAliases(const QByteArray &raw);
[[nodiscard]] QByteArray SerializeAliases(const PeerAliases &aliases);
[[nodiscard]] const QString &Alias(not_null<const PeerData*> peer);
[[nodiscard]] const QString &DisplayName(not_null<const PeerData*> peer);
[[nodiscard]] QString SetAlias(
	not_null<PeerData*> peer,
	const QString &value,
	const QString &expected);
void ShowAlias(std::shared_ptr<Main::SessionShow> show, not_null<PeerData*> peer);

inline const auto kAliases = Option<QByteArray>{
	"nagram.peerAliases", Scope::Account, QByteArray(),
	Category::Privacy, "lng_nagram_peer_alias", 0,
	[](const QByteArray &raw) { return ParseAliases(raw).has_value(); } };

inline void RegisterAliasOptions(Registry &registry) {
	Expects(registry.Add(kAliases));
}

} // namespace Nagram::Privacy
