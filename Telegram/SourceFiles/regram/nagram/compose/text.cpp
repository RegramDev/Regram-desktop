#include "regram/nagram/compose/text.h"

#include "regram/nagram/compose/options.h"
#include "regram/nagram/compose/default_format.h"
#include "history/history.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "regram/nagram/compose/spacing.h"
#include "regram/nagram/messages/reading.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/popup_menu.h"

#include <QtWidgets/QMenu>
#include "lang/lang_keys.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <algorithm>
#include <vector>

namespace Nagram::Compose {
namespace {

bool Protected(EntityType type) {
	switch (type) {
	case EntityType::Url:
	case EntityType::CustomUrl:
	case EntityType::Email:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::Mention:
	case EntityType::MentionName:
	case EntityType::CustomEmoji:
	case EntityType::BotCommand:
	case EntityType::MediaTimestamp:
	case EntityType::Phone:
	case EntityType::BankCard:
	case EntityType::Code:
	case EntityType::Pre:
	case EntityType::Blockquote:
	case EntityType::FormattedDate:
		return true;
	default:
		return false;
	}
}

} // namespace

TextWithEntities AddChineseLatinSpacing(const TextWithEntities &text) {
	const auto length = int(text.text.size());
	if (length < 2) {
		return text;
	}
	for (const auto &entity : text.entities) {
		if (!entity.validForText(length)) {
			return text;
		}
	}
	auto boundaries = std::vector<int>(length + 1);
	const auto protect = [&](const EntitiesInText &entities) {
		for (const auto &entity : entities) {
			if (Protected(entity.type()) && entity.validForText(length)) {
				++boundaries[entity.offset() + 1];
				--boundaries[entity.offset() + entity.length()];
			}
		}
	};
	protect(text.entities);
	protect(TextUtilities::ParseEntities(text.text,
		TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands
	).entities);
	auto codeStart = -1;
	auto codeTicks = 0;
	for (auto i = 0; i < length;) {
		if (text.text.at(i) == u'\\' && codeStart < 0) {
			i += std::min(2, length - i);
		} else if (text.text.at(i) != u'`') {
			++i;
		} else {
			const auto start = i;
			while (i < length && text.text.at(i) == u'`') {
				++i;
			}
			if (codeStart < 0) {
				codeStart = start;
				codeTicks = i - start;
			} else if (i - start == codeTicks) {
				++boundaries[codeStart + 1];
				--boundaries[i];
				codeStart = -1;
			}
		}
	}
	if (codeStart >= 0) {
		++boundaries[codeStart + 1];
		--boundaries[length];
	}

	const auto spaced = InsertChineseLatinSpacing(text.text, boundaries);
	if (spaced.text == text.text) {
		return text;
	}
	auto result = TextWithEntities();
	result.text = spaced.text;
	for (const auto &entity : text.entities) {
		const auto start = spaced.after[entity.offset()];
		const auto end = spaced.before[entity.offset() + entity.length()];
		result.entities.push_back(EntityInText(
			entity.type(), start, end - start, entity.data()));
	}
	return result;
}

TextWithEntities PrepareText(const TextWithEntities &text, bool editing, History *history) {
	const auto prefs = history ? PreferencesForChat(
		ForAccount(&history->session()).Get(kChatPreferences),
		history->session().userId().bare,
		SerializePeerId(history->peer->id)) : ChatPreferences();
	const auto enabled = !editing && prefs.spacing >= 0 ? (prefs.spacing == 1)
		: ForDevice().Get(editing ? kSpaceOnEdit : kSpaceOnSend);
	auto converted = text;
	if (const auto chinese = ForDevice().Get(kInputChinese)) {
		if (auto result = Messages::ConvertChinese(text, chinese == 2)) {
			converted = std::move(*result);
		} else {
			LOG(("Nagram: Chinese conversion unavailable; text sent as typed."));
		}
	}
	auto result = enabled ? AddChineseLatinSpacing(converted) : converted;
	const auto language = ForDevice().Get(kDefaultCodeLanguage);
	if (!language.isEmpty()) {
		for (auto &entity : result.entities) {
			if (entity.type() == EntityType::Pre && entity.data().isEmpty()) {
				entity = EntityInText(entity.type(), entity.offset(),
					entity.length(), language);
			}
		}
	}
	if (!editing) {
		const auto account = history
			? ForAccount(&history->session()).Get(kAccountFormatting)
			: -1;
		const auto format = prefs.formatting >= 0 ? prefs.formatting
			: account >= 0 ? account : ForDevice().Get(kDefaultFormatting);
		result = ApplyDefaultFormat(std::move(result), DefaultFormat(format));
	}
	return result;
}

QStringList QuickReplies() {
	const auto raw = ForDevice().Get(kQuickReplies);
	const auto document = QJsonDocument::fromJson(raw);
	if (!document.isObject()) {
		return {};
	}
	const auto values = document.object().value(u"replies"_q).toArray();
	if (values.size() != 2 || !values[0].isString() || !values[1].isString()) {
		return {};
	}
	return { values[0].toString(), values[1].toString() };
}

bool SetQuickReplies(const QStringList &replies) {
	if (replies.size() != 2) {
		return false;
	}
	const auto raw = QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"replies"_q, QJsonArray{ replies[0], replies[1] } },
	}).toJson(QJsonDocument::Compact);
	return ForDevice().Set(kQuickReplies, raw);
}

void InstallQuickReplies(not_null<Ui::InputField*> field) {
	field->addContextMenuHook([=](Ui::InputField::ContextMenuRequest request) {
		const auto replies = QuickReplies();
		for (auto i = 0; i != replies.size(); ++i) {
			const auto reply = replies[i];
			if (reply.isEmpty()) {
				continue;
			}
			request.menu->addAction(tr::lng_nagram_quick_reply_insert(
				tr::now, lt_index, QString::number(i + 1)), field, [=] {
				auto cursor = field->textCursor();
				cursor.beginEditBlock();
				cursor.insertText(reply);
				cursor.endEditBlock();
				field->setTextCursor(cursor);
			});
		}
	});
}

QString FormatMenuItemTitle(int index) {
	switch (index) {
	case 0: return tr::lng_menu_formatting_bold(tr::now);
	case 1: return tr::lng_menu_formatting_italic(tr::now);
	case 2: return tr::lng_menu_formatting_underline(tr::now);
	case 3: return tr::lng_menu_formatting_strike_out(tr::now);
	case 4: return tr::lng_menu_formatting_monospace(tr::now);
	case 5: return tr::lng_menu_formatting_spoiler(tr::now);
	case 6: return tr::lng_menu_formatting_blockquote(tr::now);
	case 7: return tr::lng_menu_formatting_link_create(tr::now);
	case 8: return tr::lng_menu_formatting_clear(tr::now);
	}
	Unexpected("Nagram format menu item index.");
}

void InstallFieldHooks(not_null<Ui::InputField*> field) {
	InstallQuickReplies(field);
	field->addContextMenuHook([=](Ui::InputField::ContextMenuRequest request) {
		const auto hidden = ForDevice().Get(kHiddenFormatItems);
		if (!hidden) {
			return;
		}
		auto prefixes = QStringList();
		for (auto i = 0; i != kFormatMenuItemCount; ++i) {
			if (hidden & (1 << i)) {
				prefixes.push_back(FormatMenuItemTitle(i));
			}
		}
		if (hidden & (1 << 7)) {
			prefixes.push_back(tr::lng_menu_formatting_link_edit(tr::now));
		}
		const auto title = tr::lng_menu_formatting(tr::now);
		for (const auto &action : request.menu->actions()) {
			const auto submenu = action->menu();
			if (!submenu || action->text() != title) {
				continue;
			}
			for (const auto &item : submenu->actions()) {
				const auto text = item->text();
				for (const auto &prefix : prefixes) {
					if (text.startsWith(prefix)) {
						submenu->removeAction(item);
						break;
					}
				}
			}
			action->setVisible(!submenu->actions().isEmpty());
		}
	});
}

} // namespace Nagram::Compose
