#include "regram/nagram/privacy/profile.h"

#include "regram/nagram/privacy/options.h"
#include "regram/nagram/privacy/registration_model.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/image/image_location.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

namespace Nagram::Privacy {
namespace {

[[nodiscard]] bool RegistrationKnown(not_null<PeerData*> peer) {
	return ShowsRegistration(
		true,
		peer->isUser(),
		peer->registrationMonth(),
		peer->registrationYear());
}

[[nodiscard]] QString EstimatedRegistration(not_null<PeerData*> peer) {
	const auto estimate = EstimateRegistration(peerToUser(peer->id).bare);
	const auto parsed = QDateTime::fromSecsSinceEpoch(
		estimate.date,
		QTimeZone::utc()).date();
	const auto date = langMonthOfYearFull(parsed.month(), parsed.year());
	switch (estimate.bound) {
	case RegistrationBound::Before:
		return tr::lng_nagram_registration_before(tr::now, lt_date, date);
	case RegistrationBound::About:
		return tr::lng_nagram_registration_about(tr::now, lt_date, date);
	case RegistrationBound::After:
		return tr::lng_nagram_registration_after(tr::now, lt_date, date);
	}
	Unexpected("Bound in EstimatedRegistration.");
}

} // namespace

rpl::producer<TextWithEntities> ProfileIdValue(not_null<PeerData*> peer) {
	return ForDevice().Value(kProfileIdFormat) | rpl::map([=](int format) {
		if (!format) {
			return TextWithEntities();
		}
		const auto raw = peer->id.value & PeerId::kChatTypeMask;
		auto result = QString::number(raw);
		if (format == 1) {
			if (peer->isChat()) {
				result.prepend('-');
			} else if (peer->isChannel()) {
				result = QString::number(-1000000000000LL - qint64(raw));
			}
		}
		return TextWithEntities{ result };
	});
}

rpl::producer<TextWithEntities> ProfileDcValue(not_null<PeerData*> peer) {
	return rpl::combine(
		ForDevice().Value(kShowProfileDc),
		peer->session().changes().peerFlagsValue(
			peer, Data::PeerUpdate::Flag::Photo)
	) | rpl::map([=](bool show, const auto &) {
		if (!show) {
			return TextWithEntities();
		}
		const auto location = peer->userpicLocation();
		const auto file = std::get_if<StorageFileLocation>(
			&location.file().data);
		return (file && file->dcId() > 0)
			? TextWithEntities{ QString::number(file->dcId()) }
			: TextWithEntities();
	});
}

rpl::producer<QString> ProfileRegistrationLabel(not_null<PeerData*> peer) {
	return rpl::single(0) | rpl::then(
		peer->barSettingsValue() | rpl::map_to(0)
	) | rpl::map([=](int) {
		return RegistrationKnown(peer)
			? tr::lng_nagram_profile_registration()
			: tr::lng_nagram_profile_registration_estimated();
	}) | rpl::flatten_latest();
}

rpl::producer<TextWithEntities> ProfileRegistrationValue(
		not_null<PeerData*> peer) {
	return rpl::combine(
		ForDevice().Value(kShowRegistrationDate),
		rpl::single(0) | rpl::then(
			peer->barSettingsValue() | rpl::map_to(0))
	) | rpl::map([=](bool show, int) {
		if (!show || !peer->isUser()) {
			return TextWithEntities();
		} else if (RegistrationKnown(peer)) {
			return TextWithEntities{ langMonthOfYearFull(
				peer->registrationMonth(),
				peer->registrationYear()) };
		}
		return TextWithEntities{ EstimatedRegistration(peer) };
	});
}

} // namespace Nagram::Privacy
