#include "regram/nagram/settings/network.h"

#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/restart.h"
#include "regram/nagram/network/options.h"
#include "regram/nagram/network/runtime.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class NetworkSection final : public Section<NetworkSection> {
public:
	NetworkSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_network();
	}

	static const SectionBuildMethod kBuild;
};

void AddToggle(
		SectionBuilder &builder,
		const Option<bool> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	const auto controller = builder.controller();
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(option),
		.keywords = std::move(keywords),
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([option, controller](bool value) {
			Expects(ForDevice().Set(option, value));
			if (option.flags & static_cast<unsigned>(Flag::RequiresRestart)) {
				ShowRestartPrompt(controller);
			}
		}, button->lifetime());
	}
}

QString IpStrategyLabel(int value) {
	using Strategy = Network::IpStrategy;
	switch (static_cast<Strategy>(value)) {
	case Strategy::Follow: return tr::lng_nagram_preview_follow(tr::now);
	case Strategy::Ipv4Only:
		return tr::lng_nagram_ip_strategy_ipv4_only(tr::now);
	case Strategy::PreferIpv4:
		return tr::lng_nagram_ip_strategy_prefer_ipv4(tr::now);
	case Strategy::PreferIpv6:
		return tr::lng_nagram_ip_strategy_prefer_ipv6(tr::now);
	case Strategy::Ipv6Only:
		return tr::lng_nagram_ip_strategy_ipv6_only(tr::now);
	}
	Unexpected("Invalid Nagram IP strategy.");
}

void IpStrategyBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_ip_strategy());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Network::kIpStrategy));
	for (auto value = 0; value != Network::kIpStrategyCount; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, IpStrategyLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Network::kIpStrategy, value));
		box->closeBox();
	});
}

const auto kDownloadBoosts = std::array{
	u"none"_q,
	u"balanced"_q,
	u"fast"_q,
};

QString DownloadBoostLabel(const QString &value) {
	return (value == kDownloadBoosts[1])
		? tr::lng_nagram_download_speed_boost_balanced(tr::now)
		: (value == kDownloadBoosts[2])
		? tr::lng_nagram_download_speed_boost_fast(tr::now)
		: tr::lng_nagram_preview_follow(tr::now);
}

void DownloadBoostBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_nagram_download_speed_boost());
	const auto current = int(ranges::find(
		kDownloadBoosts,
		ForDevice().Get(Network::kDownloadSpeedBoost)
	) - begin(kDownloadBoosts));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(current);
	for (auto index = 0; index != int(kDownloadBoosts.size()); ++index) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, index, DownloadBoostLabel(kDownloadBoosts[index]),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int index) {
		Expects(ForDevice().Set(
			Network::kDownloadSpeedBoost,
			kDownloadBoosts[index]));
		ShowRestartPrompt(controller);
		box->closeBox();
	});
}

void CustomDohBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_custom_doh());
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_custom_doh_placeholder(),
		ForDevice().Get(Network::kCustomDoh)));
	field->setMaxLength(Network::kMaxDohAddressLength);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		const auto value = field->getLastText().trimmed();
		if (!ForDevice().Set(Network::kCustomDoh, value)) {
			field->showError();
			box->showToast(tr::lng_nagram_custom_doh_invalid(tr::now));
			return;
		}
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = NetworkSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_network,
	.icon = &st::menuIconNetwork,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addSubsectionTitle({
		.id = u"regram/nagram/network/connection"_q,
		.title = tr::lng_nagram_network_connection(),
		.keywords = { u"connection"_q, u"network"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/network/ip-strategy"_q,
		.title = tr::lng_nagram_ip_strategy(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Network::kIpStrategy)
			| rpl::map(IpStrategyLabel),
		.onClick = [=] {
			controller->show(Box(IpStrategyBox));
		},
		.keywords = { u"IP version"_q, u"IPv4"_q, u"IPv6"_q },
	});
	builder.addDividerText(tr::lng_nagram_ip_strategy_about());
	AddToggle(builder, Network::kDisableBackupAddresses,
		tr::lng_nagram_disable_backup_addresses(),
		u"regram/nagram/network/disable-backup-addresses"_q,
		{ u"backup"_q, u"server"_q, u"address"_q });
	builder.addDividerText(tr::lng_nagram_disable_backup_addresses_about());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/network/dns"_q,
		.title = tr::lng_nagram_network_dns(),
		.keywords = { u"DNS"_q, u"domain"_q },
	});
	AddToggle(builder, Network::kUseSystemDns,
		tr::lng_nagram_use_system_dns(),
		u"regram/nagram/network/use-system-dns"_q,
		{ u"DNS"_q, u"proxy"_q, u"resolve"_q });
	builder.addDividerText(tr::lng_nagram_use_system_dns_about());
	builder.addButton({
		.id = u"regram/nagram/network/custom-doh"_q,
		.title = tr::lng_nagram_custom_doh(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Network::kCustomDoh)
			| rpl::map([](const QString &value) {
				return value.isEmpty()
					? tr::lng_nagram_preview_follow(tr::now)
					: value;
			}),
		.onClick = [=] {
			controller->show(Box(CustomDohBox));
		},
		.keywords = { u"DoH"_q, u"DNS"_q, u"HTTPS"_q },
	});
	builder.addDividerText(rpl::combine(
		tr::lng_nagram_custom_doh_about(),
		Network::CustomDohFailureValue()
	) | rpl::map([](const QString &about, const QString &failure) {
		return failure.isEmpty() ? about : (about + u"\n\n"_q + failure);
	}));
	builder.addSubsectionTitle({
		.id = u"regram/nagram/network/transfer"_q,
		.title = tr::lng_nagram_network_transfer(),
		.keywords = { u"transfer"_q, u"download"_q, u"upload"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/network/download-speed-boost"_q,
		.title = tr::lng_nagram_download_speed_boost(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Network::kDownloadSpeedBoost)
			| rpl::map(DownloadBoostLabel),
		.onClick = [=] {
			controller->show(Box(DownloadBoostBox, controller));
		},
		.keywords = { u"download"_q, u"acceleration"_q, u"speed"_q },
	});
	builder.addDividerText(tr::lng_nagram_download_speed_boost_about());
	AddToggle(builder, Network::kUploadSpeedBoost,
		tr::lng_nagram_upload_speed_boost(),
		u"regram/nagram/network/upload-speed-boost"_q,
		{ u"upload"_q, u"acceleration"_q, u"speed"_q });
	builder.addDividerText(tr::lng_nagram_upload_speed_boost_about());
});

const SectionBuildMethod NetworkSection::kBuild = kMeta.build;

} // namespace

Settings::Type NetworkId() {
	return NetworkSection::Id();
}

} // namespace Nagram
