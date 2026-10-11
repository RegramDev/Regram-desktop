#pragma once

#include "base/basic_types.h"

#include <gsl/pointers>
#include <rpl/producer.h>

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Nagram::Notifications {

[[nodiscard]] QString PolicyLabel(const QString &policy);
void PolicyBox(
	gsl::not_null<Ui::GenericBox*> box,
	gsl::not_null<Main::Session*> session,
	bool pinned,
	uint64 peer);

[[nodiscard]] rpl::producer<QString> QuietHoursLabel();
[[nodiscard]] rpl::producer<QString> KeywordAlertsLabel(
	gsl::not_null<Main::Session*> session);
void QuietHoursBox(gsl::not_null<Ui::GenericBox*> box);
void KeywordAlertsBox(
	gsl::not_null<Ui::GenericBox*> box,
	gsl::not_null<Main::Session*> session);

} // namespace Nagram::Notifications
