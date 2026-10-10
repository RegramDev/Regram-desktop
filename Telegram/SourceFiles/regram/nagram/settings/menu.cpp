#include "regram/nagram/settings/menu.h"

#include "regram/nagram/menu/model.h"
#include "regram/nagram/settings/home.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class MenuSection final : public Section<MenuSection> {
public:
	MenuSection(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_menu();
	}

	static const SectionBuildMethod kBuild;
};

QString Title(Menu::ActionId id) {
	switch (id) {
	case Menu::ActionId::Reply: return tr::lng_nagram_menu_reply(tr::now);
	case Menu::ActionId::Edit: return tr::lng_nagram_menu_edit(tr::now);
	case Menu::ActionId::Copy: return tr::lng_nagram_menu_copy(tr::now);
	case Menu::ActionId::CopyLink: return tr::lng_nagram_menu_copy_link(tr::now);
	case Menu::ActionId::Forward: return tr::lng_nagram_menu_forward(tr::now);
	case Menu::ActionId::Translate: return tr::lng_nagram_menu_translate(tr::now);
	case Menu::ActionId::Pin: return tr::lng_nagram_menu_pin(tr::now);
	case Menu::ActionId::Select: return tr::lng_nagram_menu_select(tr::now);
	case Menu::ActionId::Statistics: return tr::lng_nagram_menu_statistics(tr::now);
	case Menu::ActionId::Report: return tr::lng_nagram_menu_report(tr::now);
	case Menu::ActionId::BlockSender: return tr::lng_nagram_menu_block_sender(tr::now);
	case Menu::ActionId::Image: return tr::lng_nagram_menu_image(tr::now);
	case Menu::ActionId::Delete: return tr::lng_nagram_menu_delete(tr::now);
	case Menu::ActionId::StickerPack: return tr::lng_nagram_menu_sticker_pack(tr::now);
	case Menu::ActionId::Repeat: return tr::lng_nagram_menu_repeat(tr::now);
	case Menu::ActionId::RepeatAsCopy: return tr::lng_nagram_menu_repeat_as_copy(tr::now);
	case Menu::ActionId::ForwardWithoutQuote: return tr::lng_nagram_menu_forward_without_quote(tr::now);
	case Menu::ActionId::Batch: return tr::lng_nagram_menu_batch(tr::now);
	case Menu::ActionId::SelectSender: return tr::lng_nagram_menu_select_sender(tr::now);
	case Menu::ActionId::MediaInfo: return tr::lng_nagram_menu_media_info(tr::now);
	case Menu::ActionId::Screenshot: return tr::lng_nagram_menu_screenshot(tr::now);
	case Menu::ActionId::Reading: return tr::lng_nagram_menu_reading(tr::now);
	case Menu::ActionId::FilterAuthor: return tr::lng_nagram_filter_author_hide(tr::now);
	case Menu::ActionId::DeleteDownload: return tr::lng_nagram_menu_delete_download(tr::now);
	case Menu::ActionId::QuickRating: return tr::lng_nagram_menu_quick_rating(tr::now);
	case Menu::ActionId::SelectBetween: return tr::lng_nagram_menu_select_between(tr::now);
	case Menu::ActionId::SeenBy: return tr::lng_nagram_menu_seen_by(tr::now);
	case Menu::ActionId::MessageDetails: return tr::lng_nagram_menu_details(tr::now);
	case Menu::ActionId::HideMessage: return tr::lng_nagram_hide_message(tr::now);
	case Menu::ActionId::SaveToSaved: return tr::lng_nagram_menu_save_to_saved(tr::now);
	case Menu::ActionId::SelectAll: return tr::lng_nagram_menu_select_all(tr::now);
	case Menu::ActionId::Summarize: return tr::lng_nagram_menu_summarize(tr::now);
	case Menu::ActionId::TranscribeSelected: return tr::lng_nagram_menu_transcribe_selected(tr::now);
	default: return QString();
	}
}

QString VisibilityLabel(Menu::Visibility visibility) {
	switch (visibility) {
	case Menu::Visibility::Hide: return tr::lng_nagram_menu_hidden(tr::now);
	case Menu::Visibility::WithOption: return tr::lng_nagram_menu_with_option(tr::now);
	default: return tr::lng_nagram_menu_shown(tr::now);
	}
}

const style::icon *Icon(Menu::ActionId id) {
	switch (id) {
	case Menu::ActionId::Reply: return &st::menuIconReply;
	case Menu::ActionId::Edit: return &st::menuIconEdit;
	case Menu::ActionId::Copy: return &st::menuIconCopy;
	case Menu::ActionId::CopyLink: return &st::menuIconLink;
	case Menu::ActionId::Forward: return &st::menuIconForward;
	case Menu::ActionId::Translate: return &st::menuIconTranslate;
	case Menu::ActionId::Pin: return &st::menuIconPin;
	case Menu::ActionId::Select: return &st::menuIconSelect;
	case Menu::ActionId::Statistics: return &st::menuIconStats;
	case Menu::ActionId::Report: return &st::menuIconReport;
	case Menu::ActionId::BlockSender: return &st::menuIconBlock;
	case Menu::ActionId::Image: return &st::menuIconSaveImage;
	case Menu::ActionId::Delete: return &st::menuIconDelete;
	case Menu::ActionId::StickerPack: return &st::menuIconStickers;
	case Menu::ActionId::Repeat:
	case Menu::ActionId::RepeatAsCopy: return &st::menuIconRepeat;
	case Menu::ActionId::ForwardWithoutQuote: return &st::menuIconForward;
	case Menu::ActionId::Batch: return &st::menuIconCopy;
	case Menu::ActionId::SelectSender: return &st::menuIconSelect;
	case Menu::ActionId::MediaInfo: return &st::menuIconInfo;
	case Menu::ActionId::Screenshot: return &st::menuIconSaveImage;
	case Menu::ActionId::Reading: return &st::menuIconTranslate;
	case Menu::ActionId::FilterAuthor: return &st::menuIconBlock;
	case Menu::ActionId::DeleteDownload: return &st::menuIconClear;
	case Menu::ActionId::QuickRating: return &st::menuIconReply;
	case Menu::ActionId::SelectBetween: return &st::menuIconSelect;
	case Menu::ActionId::SeenBy: return &st::menuIconShowInChat;
	case Menu::ActionId::MessageDetails: return &st::menuIconInfo;
	case Menu::ActionId::HideMessage: return &st::menuIconStealth;
	case Menu::ActionId::SaveToSaved: return &st::menuIconSavedMessages;
	case Menu::ActionId::SelectAll: return &st::menuIconSelect;
	case Menu::ActionId::TranscribeSelected: return &st::menuIconSoundOn;
	default: return &st::menuIconChatBubble;
	}
}

void VisibilityBox(not_null<Ui::GenericBox*> box, Menu::ActionId id) {
	box->setTitle(Title(id));
	const auto current = Menu::ReadVisibility(
		ForDevice().Get(Menu::kMenuConfig), id);
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(int(current));
	for (const auto &state : { Menu::Visibility::Show,
			Menu::Visibility::Hide, Menu::Visibility::WithOption }) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, int(state), VisibilityLabel(state),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		const auto current = ForDevice().Get(Menu::kMenuConfig);
		Expects(ForDevice().Set(Menu::kMenuConfig, Menu::WriteVisibility(
			current, id, Menu::Visibility(value))));
		box->closeBox();
	});
}

void QuickRatingBox(
		not_null<Ui::GenericBox*> box,
		const Option<QString> *option,
		rpl::producer<QString> title) {
	box->setTitle(std::move(title));
	const auto field = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			tr::lng_nagram_menu_quick_rating_placeholder(),
			ForDevice().Get(*option)),
		st::boxRowPadding);
	field->setMaxLength(64);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		if (ForDevice().Set(*option, field->getLastText().trimmed())) {
			box->closeBox();
		}
	};
	field->submits() | rpl::on_next(submit, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

const auto kMeta = BuildHelper({
	.id = MenuSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_menu,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto confirm = builder.addButton({
		.id = u"regram/nagram/menu/confirm-repeat"_q,
		.title = tr::lng_nagram_menu_confirm_repeat(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(Menu::kConfirmRepeat),
		.keywords = { u"repeat"_q, u"confirm"_q },
	});
	if (confirm) {
		confirm->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(Menu::kConfirmRepeat, value));
		}, confirm->lifetime());
	}
	const auto addMenuToggle = [&](
			const Option<bool> *option,
			rpl::producer<QString> title,
			QString id) {
		const auto button = builder.addButton({
			.id = std::move(id),
			.title = std::move(title),
			.st = &st::settingsButtonNoIcon,
			.toggled = ForDevice().Value(*option),
		});
		if (button) {
			button->toggledChanges(
			) | rpl::on_next([=](bool value) {
				Expects(ForDevice().Set(*option, value));
			}, button->lifetime());
		}
	};
	addMenuToggle(&Menu::kCompactMenu,
		tr::lng_nagram_menu_compact(),
		u"regram/nagram/menu/compact"_q);
	addMenuToggle(&Menu::kRepeatWithoutQuote,
		tr::lng_nagram_menu_repeat_without_quote(),
		u"regram/nagram/menu/repeat-without-quote"_q);
	addMenuToggle(&Menu::kNoRepeatInChannels,
		tr::lng_nagram_menu_no_repeat_channels(),
		u"regram/nagram/menu/no-repeat-channels"_q);
	addMenuToggle(&Menu::kScrollAfterRepeat,
		tr::lng_nagram_menu_scroll_after_repeat(),
		u"regram/nagram/menu/scroll-after-repeat"_q);
	const auto addRating = [&](
			const Option<QString> *option,
			const tr::phrase<> *title,
			QString id) {
		builder.addButton({
			.id = std::move(id),
			.title = (*title)(),
			.st = &st::settingsButtonNoIcon,
			.label = ForDevice().Value(*option)
				| rpl::map([](const QString &text) {
					return text.isEmpty()
						? tr::lng_nagram_menu_quick_rating_empty(tr::now)
						: text;
				}),
			.onClick = [=] {
				controller->show(Box(QuickRatingBox, option, (*title)()));
			},
			.keywords = { u"rating"_q, u"quick reply"_q },
		});
	};
	addRating(&Menu::kQuickRatingFirst,
		&tr::lng_nagram_menu_quick_rating_first,
		u"regram/nagram/menu/quick-rating-first"_q);
	addRating(&Menu::kQuickRatingSecond,
		&tr::lng_nagram_menu_quick_rating_second,
		u"regram/nagram/menu/quick-rating-second"_q);
	for (const auto &entry : Menu::kEntries) {
		const auto id = entry.id;
		builder.addButton({
			.id = u"regram/nagram/menu/"_q + QString::fromLatin1(entry.titleKey),
			.title = rpl::single(Title(id)),
			.icon = { Icon(id) },
			.label = ForDevice().Value(Menu::kMenuConfig)
				| rpl::map([=](const QByteArray &config) {
					return VisibilityLabel(Menu::ReadVisibility(config, id));
				}),
			.onClick = [=] { controller->show(Box(VisibilityBox, id)); },
			.keywords = { Title(id) },
		});
	}
	builder.addDividerText(tr::lng_nagram_menu_note());
});

const SectionBuildMethod MenuSection::kBuild = kMeta.build;

} // namespace

Settings::Type MenuId() {
	return MenuSection::Id();
}

} // namespace Nagram
