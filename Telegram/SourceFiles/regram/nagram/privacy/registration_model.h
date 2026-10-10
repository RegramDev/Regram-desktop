#pragma once

#include <QtCore/QtGlobal>

#include <span>

namespace Nagram::Privacy {

struct RegistrationAnchor {
	quint64 id = 0;
	qint64 date = 0;
};

enum class RegistrationBound {
	Before,
	About,
	After,
};

struct RegistrationEstimate {
	RegistrationBound bound = RegistrationBound::About;
	qint64 date = 0;

	friend inline bool operator==(
		const RegistrationEstimate &,
		const RegistrationEstimate &) = default;
};

[[nodiscard]] std::span<const RegistrationAnchor> RegistrationAnchors();
[[nodiscard]] RegistrationEstimate EstimateRegistration(quint64 userId);

} // namespace Nagram::Privacy
