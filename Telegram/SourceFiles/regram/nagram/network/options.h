#pragma once

#include "regram/nagram/core/options.h"
#include "regram/nagram/network/model.h"
#include "base/basic_types.h"

namespace Nagram::Network {

inline constexpr auto kIpStrategy = Option<int>{
	"nagram.ipStrategy", Scope::Device, 0,
	Category::Network, "lng_nagram_ip_strategy",
	static_cast<unsigned>(Flag::LocalOnly), ValidIpStrategy };
inline constexpr auto kDisableBackupAddresses = Option<bool>{
	"nagram.disableBackupAddresses", Scope::Device, false,
	Category::Network, "lng_nagram_disable_backup_addresses",
	static_cast<unsigned>(Flag::LocalOnly) };
inline constexpr auto kUseSystemDns = Option<bool>{
	"nagram.useSystemDns", Scope::Device, false,
	Category::Network, "lng_nagram_use_system_dns",
	static_cast<unsigned>(Flag::LocalOnly) };
inline const auto kCustomDoh = Option<QString>{
	"nagram.customDoh", Scope::Device, QString(),
	Category::Network, "lng_nagram_custom_doh",
	static_cast<unsigned>(Flag::LocalOnly), ValidCustomDoh };
inline constexpr auto kTransferFlags = static_cast<unsigned>(Flag::LocalOnly)
	| static_cast<unsigned>(Flag::RequiresRestart);
inline const auto kDownloadSpeedBoost = Option<QString>{
	"nagram.downloadSpeedBoost", Scope::Device, u"none"_q,
	Category::Network, "lng_nagram_download_speed_boost",
	kTransferFlags, ValidDownloadBoost };
inline constexpr auto kUploadSpeedBoost = Option<bool>{
	"nagram.uploadSpeedBoost", Scope::Device, false,
	Category::Network, "lng_nagram_upload_speed_boost",
	kTransferFlags };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kIpStrategy));
	Expects(registry.Add(kDisableBackupAddresses));
	Expects(registry.Add(kUseSystemDns));
	Expects(registry.Add(kCustomDoh));
	Expects(registry.Add(kDownloadSpeedBoost));
	Expects(registry.Add(kUploadSpeedBoost));
}

} // namespace Nagram::Network
