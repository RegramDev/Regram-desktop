#include "regram/nagram/settings/home.h"

#include "regram/ayu/ui/settings/settings_main.h"
#include "regram/nagram/settings/chats.h"
#include "regram/nagram/settings/compose.h"
#include "regram/nagram/settings/config.h"
#include "regram/nagram/settings/interface.h"
#include "regram/nagram/settings/media.h"
#include "regram/nagram/settings/menu.h"
#include "regram/nagram/settings/messages.h"
#include "regram/nagram/settings/network.h"
#include "regram/nagram/settings/privacy.h"
#include "regram/nagram/settings/rules.h"
#include "regram/nagram/settings/services.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"

#include "styles/style_menu_icons.h"

namespace Nagram {

Settings::Type HomeId() {
	return ::Settings::AyuMainId();
}

void AddSettingsEntry(::Settings::Builder::SectionBuilder &builder) {
	builder.addSectionButton({
		.title = tr::lng_nagram_settings(),
		.targetSection = HomeId(),
		.icon = { &st::menuIconNagram },
		.keywords = { u"Regram"_q, u"desktop"_q },
	});
}

void AddCategoryEntries(::Settings::Builder::SectionBuilder &builder) {
	builder.addSectionButton({
		.title = tr::lng_nagram_interface(),
		.targetSection = InterfaceId(),
		.icon = { &st::menuIconPalette },
		.keywords = { u"interface"_q, u"appearance"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_messages(),
		.targetSection = MessagesId(),
		.icon = { &st::menuIconChatBubble },
		.keywords = { u"messages"_q, u"time"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_chats(),
		.targetSection = ChatsId(),
		.icon = { &st::menuIconChats },
		.keywords = { u"chats"_q, u"list"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_compose(),
		.targetSection = ComposeId(),
		.icon = { &st::menuIconShortcut },
		.keywords = { u"compose"_q, u"send"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_media(),
		.targetSection = MediaId(),
		.icon = { &st::menuIconStickers },
		.keywords = { u"media"_q, u"sticker"_q, u"emoji"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_menu(),
		.targetSection = MenuId(),
		.icon = { &st::menuIconShowAll },
		.keywords = { u"menu"_q, u"actions"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_privacy(),
		.targetSection = PrivacyId(),
		.icon = { &st::menuIconLock },
		.keywords = { u"privacy"_q, u"phone"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_services(),
		.targetSection = ServicesId(),
		.icon = { &st::menuIconTranslate },
		.keywords = { u"translation"_q, u"AI"_q, u"service"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_rules(),
		.targetSection = RulesId(),
		.icon = { &st::menuIconTagFilter },
		.keywords = { u"filter"_q, u"link"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_network(),
		.targetSection = NetworkId(),
		.icon = { &st::menuIconNetwork },
		.keywords = { u"network"_q, u"connection"_q, u"IPv6"_q },
	});
	builder.addSectionButton({
		.title = tr::lng_nagram_config_title(),
		.targetSection = ConfigId(),
		.icon = { &st::menuIconStorage },
		.keywords = { u"backup"_q, u"import"_q, u"export"_q },
	});
}

} // namespace Nagram
