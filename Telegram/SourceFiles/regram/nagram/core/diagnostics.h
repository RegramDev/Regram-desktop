#pragma once

#include "regram/nagram/core/options.h"

namespace Nagram {

inline constexpr auto kShowRpcErrors = Option<bool>{
	"nagram.showRpcErrors", Scope::Device, false,
	Category::Services, "lng_nagram_show_rpc_errors",
	static_cast<unsigned>(Flag::LocalOnly) };

inline void RegisterDiagnosticsOptions(Registry &registry) {
	Expects(registry.Add(kShowRpcErrors));
}

void InstallRpcErrorObserver();

} // namespace Nagram
