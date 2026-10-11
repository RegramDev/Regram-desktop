#include "regram/nagram/settings/menu.h"
#include "regram/ayu/ui/settings/settings_chats.h"
#include "regram/ayu/ui/settings/settings_appearance.h"

#include "regram/nagram/menu/model.h"
#include "regram/nagram/settings/home.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "crl/crl_on_main.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/vertical_layout_reorder.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_nagram_compose.h"

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
	case Menu::ActionId::Replies: return tr::lng_regram_message_replies(tr::now);
	case Menu::ActionId::RestrictSender: return tr::lng_regram_restrict_sender(tr::now);
	case Menu::ActionId::CopyMarkdown: return tr::lng_nagram_menu_copy_markdown(tr::now);
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
	case Menu::ActionId::MessageJson: return tr::lng_regram_message_json(tr::now);
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
	case Menu::ActionId::Replies: return &st::menuIconViewReplies;
	case Menu::ActionId::RestrictSender: return &st::menuIconPermissions;
	case Menu::ActionId::CopyMarkdown: return &st::menuIconCopy;
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
	case Menu::ActionId::MessageJson: return &st::menuIconInfo;
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
	const auto grouped = Menu::ReadActionList(ForDevice().Get(Menu::kRegramSubmenu));
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ranges::contains(grouped, id) ? 3 : int(current));
	for (const auto &state : { Menu::Visibility::Show,
			Menu::Visibility::Hide, Menu::Visibility::WithOption }) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, int(state), VisibilityLabel(state),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	box->addRow(object_ptr<Ui::Radiobutton>(
		box, group, 3, tr::lng_regram_menu_submenu(tr::now),
		st::settingsSendType), st::settingsSendTypePadding);
	group->setChangedCallback([=](int value) {
		auto grouped = Menu::ReadActionList(ForDevice().Get(Menu::kRegramSubmenu));
		std::erase(grouped, id);
		if (value == 3) {
			grouped.push_back(id);
		}
		Expects(ForDevice().Set(Menu::kRegramSubmenu, Menu::WriteActionList(grouped)));
		const auto current = ForDevice().Get(Menu::kMenuConfig);
		Expects(ForDevice().Set(Menu::kMenuConfig, Menu::WriteVisibility(
			current, id, value == 3 ? Menu::Visibility::Show : Menu::Visibility(value))));
		box->closeBox();
	});
}

void MenuOrderBox(not_null<Ui::GenericBox*> box) {
	struct Row {
		Menu::ActionId id;
		Ui::IconButton *handle = nullptr;
		Ui::IconButton *up = nullptr;
		Ui::IconButton *down = nullptr;
	};
	struct State {
		std::vector<Row> entries;
		std::unique_ptr<Ui::VerticalLayoutReorder> reorder;
		rpl::lifetime lifetime;
	};
	box->setTitle(tr::lng_regram_menu_order());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_regram_menu_order_about(), st::boxLabel));
	const auto state = box->lifetime().make_state<State>();
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = [=] {
		for (auto i = 0; i != int(state->entries.size()); ++i) {
			state->entries[i].up->setDisabled(i == 0);
			state->entries[i].down->setDisabled(i + 1 == int(state->entries.size()));
		}
	};
	const auto move = [=](Menu::ActionId id, int delta) {
		crl::on_main(box, [=] {
			if (!state->reorder) {
				return;
			}
			state->reorder->cancel();
			const auto from = ranges::find(state->entries, id, &Row::id) - state->entries.begin();
			const auto to = from + delta;
			if (to >= 0 && to < int(state->entries.size())) {
				rows->reorderRows(from, to);
				base::reorder(state->entries, from, to);
				refresh();
			}
			state->reorder->start();
		});
	};
	for (const auto id : Menu::ReadActionOrder(ForDevice().Get(Menu::kMenuOrder))) {
		const auto row = rows->add(object_ptr<Ui::RpWidget>(rows));
		row->setFixedHeight(st::nagramMenuOrderRowHeight);
		const auto label = Ui::CreateChild<Ui::FlatLabel>(
			row, rpl::single(Title(id)), st::boxLabel);
		const auto handle = Ui::CreateChild<Ui::IconButton>(row, st::nagramMenuOrderButton);
		const auto up = Ui::CreateChild<Ui::IconButton>(row, st::nagramMenuOrderButton);
		const auto down = Ui::CreateChild<Ui::IconButton>(row, st::nagramMenuOrderButton);
		handle->setIconOverride(&st::menuIconReorder, &st::menuIconReorder);
		handle->setCursor(Qt::OpenHandCursor);
		handle->setAccessibleName(Title(id));
		up->setIconOverride(&st::nagramMenuOrderUp, &st::nagramMenuOrderUp);
		down->setIconOverride(&st::nagramMenuOrderDown, &st::nagramMenuOrderDown);
		up->setAccessibleName(tr::lng_regram_menu_move_up(tr::now));
		down->setAccessibleName(tr::lng_regram_menu_move_down(tr::now));
		up->setClickedCallback([=] { move(id, -1); });
		down->setClickedCallback([=] { move(id, 1); });
		state->entries.push_back({ id, handle, up, down });
		row->widthValue() | rpl::on_next([=](int width) {
			const auto skip = st::nagramMenuOrderSkip;
			label->resizeToWidth(std::max(width - 3 * up->width() - 5 * skip, 0));
			handle->moveToLeft(skip, (row->height() - handle->height()) / 2);
			label->moveToLeft(handle->width() + 2 * skip, (row->height() - label->height()) / 2);
			up->moveToRight(down->width() + 2 * skip, (row->height() - up->height()) / 2);
			down->moveToRight(skip, (row->height() - down->height()) / 2);
		}, row->lifetime());
	}
	refresh();
	box->setShowFinishedCallback([=] {
		if (state->reorder) {
			return;
		}
		auto parent = rows->parentWidget();
		auto scroll = static_cast<Ui::ScrollArea*>(nullptr);
		while (parent && !scroll) {
			scroll = dynamic_cast<Ui::ScrollArea*>(parent);
			parent = parent->parentWidget();
		}
		state->reorder = scroll
			? std::make_unique<Ui::VerticalLayoutReorder>(rows, scroll)
			: std::make_unique<Ui::VerticalLayoutReorder>(rows);
		state->reorder->setMouseEventProxy([=](int index) {
			return not_null<Ui::RpWidget*>(state->entries[index].handle);
		});
		state->reorder->updates() | rpl::on_next([=](Ui::VerticalLayoutReorder::Single update) {
			if (update.state == Ui::VerticalLayoutReorder::State::Applied) {
				base::reorder(state->entries, update.oldPosition, update.newPosition);
				refresh();
			}
		}, state->lifetime);
		state->reorder->start();
	});
	box->addButton(tr::lng_settings_save(), [=] {
		if (state->reorder) {
			state->reorder->finishReordering();
		}
		auto ids = std::vector<Menu::ActionId>();
		for (const auto &entry : state->entries) {
			ids.push_back(entry.id);
		}
		Expects(ForDevice().Set(Menu::kMenuOrder, Menu::WriteActionList(ids)));
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
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
	builder.addButton({
		.id = u"regram/menu/order"_q,
		.title = tr::lng_regram_menu_order(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Box(MenuOrderBox)); },
		.keywords = { u"order"_q, u"menu"_q, u"iOS"_q },
	});
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
			.label = rpl::combine(ForDevice().Value(Menu::kMenuConfig),
				ForDevice().Value(Menu::kRegramSubmenu))
				| rpl::map([=](const QByteArray &config, const QByteArray &groups) {
					if (ranges::contains(Menu::ReadActionList(groups), id)) {
						return tr::lng_regram_menu_submenu(tr::now);
					}
					return VisibilityLabel(Menu::ReadVisibility(config, id));
				}),
			.onClick = [=] { controller->show(Box(VisibilityBox, id)); },
			.keywords = { Title(id) },
		});
	}
	builder.addDividerText(tr::lng_nagram_menu_note());
	::Settings::BuildExtraMessageMenu(builder);
	::Settings::BuildExtraMenu(builder);
});

const SectionBuildMethod MenuSection::kBuild = kMeta.build;

} // namespace

Settings::Type MenuId() {
	return MenuSection::Id();
}

} // namespace Nagram
