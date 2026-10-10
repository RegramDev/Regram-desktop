#pragma once

#include "ui/text/text_entity.h"
#include <gsl/pointers>
#include <rpl/rpl.h>

class PeerData;

namespace Nagram::Privacy {

[[nodiscard]] rpl::producer<TextWithEntities> ProfileIdValue(
	not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<TextWithEntities> ProfileDcValue(
	not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<QString> ProfileRegistrationLabel(
	not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<TextWithEntities> ProfileRegistrationValue(
	not_null<PeerData*> peer);

} // namespace Nagram::Privacy
