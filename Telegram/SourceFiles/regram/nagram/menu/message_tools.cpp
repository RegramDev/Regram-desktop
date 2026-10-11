#include "regram/nagram/menu/message_tools.h"

#include "api/api_chat_participants.h"
#include "api/api_common.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "boxes/peers/edit_participant_box.h"
#include "boxes/peers/edit_participants_box.h"
#include "crl/crl_on_main.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_poll.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_web_page.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "iv/iv_rich_page.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/mtproto_response.h"
#include "regram/nagram/menu/actions.h"
#include "regram/nagram/messages/markdown.h"
#include "regram/nagram/privacy/protection.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QTextEdit>

#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_nagram_compose.h"
#include "styles/style_widgets.h"

namespace Nagram::Menu {
namespace {

QString FormatDate(TimeId date) {
	return base::unixtime::parse(date).toString(Qt::ISODate);
}

QString PeerLine(not_null<PeerData*> peer) {
	return peer->name() + u" ("_q + QString::number(peer->id.value) + u')';
}

QString Details(not_null<HistoryItem*> item) {
	auto lines = QStringList();
	const auto add = [&](QString name, QString value) {
		if (!value.isEmpty()) {
			lines.push_back(name + u": "_q + value);
		}
	};
	add(tr::lng_nagram_details_message_id(tr::now),
		QString::number(item->id.bare));
	add(tr::lng_nagram_details_chat(tr::now), PeerLine(item->history()->peer));
	add(tr::lng_nagram_details_sender(tr::now), PeerLine(item->from()));
	add(tr::lng_nagram_details_date(tr::now), FormatDate(item->date()));
	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		add(tr::lng_nagram_details_edited(tr::now), FormatDate(edited->date));
	}
	if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
		if (forwarded->originalSender) {
			add(tr::lng_nagram_details_forwarded_from(tr::now),
				PeerLine(forwarded->originalSender));
		} else if (!forwarded->originalPostAuthor.isEmpty()) {
			add(tr::lng_nagram_details_forwarded_from(tr::now),
				forwarded->originalPostAuthor);
		}
		if (forwarded->originalDate) {
			add(tr::lng_nagram_details_forwarded_date(tr::now),
				FormatDate(forwarded->originalDate));
		}
		if (forwarded->originalId) {
			add(tr::lng_nagram_details_forwarded_id(tr::now),
				QString::number(forwarded->originalId.bare));
		}
	}
	if (const auto bot = item->viaBot()) {
		add(tr::lng_nagram_details_via_bot(tr::now), PeerLine(bot));
	}
	if (const auto group = item->groupId()) {
		add(tr::lng_nagram_details_album(tr::now),
			QString::number(group.value));
	}
	if (const auto views = item->viewsCount(); views >= 0) {
		add(tr::lng_nagram_details_views(tr::now), QString::number(views));
	}
	return lines.join(u'\n');
}

void ShowDetails(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	const auto item = controller->session().data().message(itemId);
	if (!item) {
		return;
	}
	const auto text = Details(item);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_menu_details());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, text, st::boxLabel))->setSelectable(true);
		box->addButton(tr::lng_nagram_details_copy(), [=] {
			QGuiApplication::clipboard()->setText(text);
			box->showToast(tr::lng_nagram_details_copied(tr::now));
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

QJsonObject TextJson(const TextWithEntities &text) {
	auto entities = QJsonArray();
	for (const auto &entity : text.entities) {
		entities.push_back(QJsonObject{
			{ u"type"_q, int(entity.type()) },
			{ u"offset"_q, entity.offset() },
			{ u"length"_q, entity.length() },
			{ u"data"_q, entity.data() },
		});
	}
	return { { u"text"_q, text.text }, { u"entities"_q, entities } };
}

QJsonArray BlocksJson(
		const std::vector<Iv::RichPage::Block> &blocks,
		int &budget,
		bool &truncated,
		int depth = 0) {
	auto result = QJsonArray();
	if (depth > 32) {
		truncated = true;
		return result;
	}
	for (const auto &block : blocks) {
		if (!budget--) {
			budget = 0;
			truncated = true;
			break;
		}
		auto lists = QJsonArray();
		for (const auto &item : block.listItems) {
			lists.push_back(QJsonObject{
				{ u"text"_q, TextJson(item.text.text) },
				{ u"taskState"_q, int(item.taskState) },
				{ u"anchorId"_q, item.anchorId },
				{ u"blocks"_q, BlocksJson(item.blocks, budget, truncated, depth + 1) },
			});
		}
		auto rows = QJsonArray();
		for (const auto &row : block.tableRows) {
			auto cells = QJsonArray();
			for (const auto &cell : row.cells) {
				cells.push_back(QJsonObject{
					{ u"text"_q, TextJson(cell.text.text) },
					{ u"colspan"_q, cell.colspan },
					{ u"rowspan"_q, cell.rowspan },
					{ u"header"_q, cell.header },
				});
			}
			rows.push_back(cells);
		}
		result.push_back(QJsonObject{
			{ u"kind"_q, int(block.kind) },
			{ u"text"_q, TextJson(block.text.text) },
			{ u"caption"_q, TextJson(block.caption.text) },
			{ u"anchorId"_q, block.anchorId },
			{ u"url"_q, block.url },
			{ u"language"_q, block.language },
			{ u"formula"_q, block.formula },
			{ u"headingLevel"_q, block.headingLevel },
			{ u"photoId"_q, QString::number(block.photoId) },
			{ u"documentId"_q, QString::number(block.documentId) },
			{ u"spoiler"_q, block.spoiler },
			{ u"collapsed"_q, block.collapsed },
			{ u"listItems"_q, lists },
			{ u"tableRows"_q, rows },
			{ u"blocks"_q, BlocksJson(block.blocks, budget, truncated, depth + 1) },
		});
	}
	return result;
}

QJsonObject MediaJson(not_null<Data::Media*> media) {
	auto result = QJsonObject{
		{ u"spoiler"_q, media->hasSpoiler() },
		{ u"ttlSeconds"_q, double(media->ttlSeconds()) },
	};
	if (const auto document = media->document()) {
		result.insert(u"document"_q, QJsonObject{
			{ u"id"_q, QString::number(document->id) },
			{ u"name"_q, document->filename() },
			{ u"mime"_q, document->mimeString() },
			{ u"bytes"_q, QString::number(document->size) },
			{ u"width"_q, document->dimensions.width() },
			{ u"height"_q, document->dimensions.height() },
		});
	}
	if (const auto photo = media->photo()) {
		result.insert(u"photo"_q, QJsonObject{
			{ u"id"_q, QString::number(photo->id) },
			{ u"width"_q, photo->width() },
			{ u"height"_q, photo->height() },
		});
	}
	if (const auto webpage = media->webpage()) {
		result.insert(u"webpage"_q, QJsonObject{
			{ u"id"_q, QString::number(webpage->id) },
			{ u"url"_q, webpage->url },
		});
	}
	if (const auto poll = media->poll()) {
		auto answers = QJsonArray();
		for (const auto &answer : poll->answers) {
			answers.push_back(QJsonObject{
				{ u"text"_q, TextJson(answer.text) },
				{ u"option"_q, QString::fromLatin1(answer.option.toHex()) },
				{ u"votes"_q, answer.votes },
				{ u"chosen"_q, answer.chosen },
			});
		}
		result.insert(u"poll"_q, QJsonObject{
			{ u"id"_q, QString::number(poll->id) },
			{ u"question"_q, TextJson(poll->question) },
			{ u"answers"_q, answers },
			{ u"totalVoters"_q, poll->totalVoters },
		});
	}
	return result;
}

void ShowMessageJson(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	const auto item = controller->session().data().message(itemId);
	if (!item) {
		return;
	}
	const auto text = TextJson(item->originalText());
	const auto reply = item->replyTo();
	auto data = QJsonObject{
		{ u"source"_q, u"Regram local message model"_q },
		{ u"id"_q, QString::number(item->id.bare) },
		{ u"peerId"_q, QString::number(item->history()->peer->id.value) },
		{ u"senderId"_q, QString::number(item->from()->id.value) },
		{ u"date"_q, FormatDate(item->date()) },
		{ u"outgoing"_q, item->out() },
		{ u"text"_q, text.value(u"text"_q) },
		{ u"entities"_q, text.value(u"entities"_q) },
		{ u"replyToMessageId"_q, QString::number(reply.messageId.msg.bare) },
		{ u"topicRootId"_q, QString::number(reply.topicRootId.bare) },
	};
	data.insert(u"albumId"_q, QString::number(item->groupId().value));
	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		data.insert(u"editedDate"_q, FormatDate(edited->date));
	}
	if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
		data.insert(u"forwarded"_q, QJsonObject{
			{ u"date"_q, FormatDate(forwarded->originalDate) },
			{ u"messageId"_q, QString::number(forwarded->originalId.bare) },
			{ u"senderId"_q, forwarded->originalSender
				? QString::number(forwarded->originalSender->id.value) : QString() },
			{ u"author"_q, forwarded->originalPostAuthor },
		});
	}
	if (const auto media = item->media()) {
		data.insert(u"media"_q, MediaJson(media));
	}
	if (const auto page = item->richPage()) {
		auto budget = 2000;
		auto truncated = false;
		const auto blocks = BlocksJson(page->blocks, budget, truncated);
		data.insert(u"richPage"_q, QJsonObject{
			{ u"url"_q, page->url },
			{ u"rtl"_q, page->rtl },
			{ u"partial"_q, page->part },
			{ u"blocks"_q, blocks },
			{ u"truncated"_q, truncated },
		});
	}
	const auto json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Indented));
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_regram_message_json());
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, Ui::InputField::Mode::MultiLine,
			rpl::single(QString()), json));
		field->setMinHeight(st::nagramMessageJsonHeight);
		field->setMaxHeight(st::nagramMessageJsonHeight);
		field->rawTextEdit()->setReadOnly(true);
		box->addButton(tr::lng_nagram_details_copy(), [=] {
			QGuiApplication::clipboard()->setText(json);
			box->showToast(tr::lng_nagram_details_copied(tr::now));
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void RestrictSender(
		not_null<Window::SessionController*> controller,
		FullMsgId id) {
	const auto item = controller->session().data().message(id);
	const auto channel = item ? item->history()->peer->asChannel() : nullptr;
	const auto user = item ? item->from()->asUser() : nullptr;
	if (!channel || !user || user->isSelf() || !channel->canRestrictParticipant(user)) {
		return;
	}
	const auto session = &controller->session();
	session->api().request(MTPchannels_GetParticipant(
		channel->inputChannel(), user->input()
	)).done(crl::guard(controller, [=](const MTPchannels_ChannelParticipant &result) {
		const auto &data = result.data();
		session->data().processUsers(data.vusers());
		session->data().processChats(data.vchats());
		const auto participant = Api::ChatParticipant(data.vparticipant(), channel);
		if (participant.id() != user->id || participant.isCreator()
			|| (participant.isCreatorOrAdmin()
				&& !channel->amCreator() && !participant.canBeEdited())
			|| !channel->canRestrictParticipant(user)) {
			return;
		}
		const auto by = session->data().userLoaded(participant.by());
		auto box = Box<EditRestrictedBox>(channel, user, participant.isCreatorOrAdmin(),
			participant.restrictions(), participant.rank(), by, participant.restrictedSince());
		const auto weak = base::make_weak(box.data());
		box->setSaveCallback(SaveRestrictedCallback(controller->uiShow(), channel, user,
			crl::guard(box.data(), [=](ChatRestrictionsInfo) {
				if (const auto strong = weak.get()) {
					strong->closeBox();
				}
			}), crl::guard(box.data(), [=] {
				if (const auto strong = weak.get()) {
					strong->closeBox();
				}
			})));
		controller->show(std::move(box));
	})).fail(crl::guard(controller, [=](const MTP::Error &error) {
		if (!MTP::IgnoreError(error)) {
			controller->showToast(error.type());
		}
	})).send();
}

bool CanSaveToSaved(not_null<HistoryItem*> item) {
	return item->isRegular()
		&& item->allowsForward()
		&& !item->history()->peer->isSelf();
}

void SaveToSaved(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	auto &session = controller->session();
	const auto item = session.data().message(itemId);
	if (!item || !CanSaveToSaved(item)) {
		controller->showToast(tr::lng_nagram_menu_batch_unavailable(tr::now));
		return;
	}
	const auto history = item->history();
	auto resolved = history->resolveForwardDraft(Data::ForwardDraft{
		.ids = history->owner().itemOrItsGroup(item),
		.options = Data::ForwardOptions::PreserveInfo,
	});
	if (resolved.items.empty()) {
		return;
	}
	auto action = Api::SendAction(session.data().history(session.user()));
	action.clearDraft = false;
	action.generateLocal = false;
	session.api().forwardMessages(
		std::move(resolved),
		action,
		crl::guard(controller, [=] {
			controller->showToast(tr::lng_nagram_menu_saved_done(tr::now));
		}));
}

[[nodiscard]] std::optional<Markdown::Style> MarkdownStyle(EntityType type) {
	using Style = Markdown::Style;
	switch (type) {
	case EntityType::Bold:
	case EntityType::Semibold: return Style::Bold;
	case EntityType::Italic: return Style::Italic;
	case EntityType::Underline: return Style::Underline;
	case EntityType::StrikeOut: return Style::Strike;
	case EntityType::Spoiler: return Style::Spoiler;
	case EntityType::CustomUrl:
	case EntityType::MentionName: return Style::Link;
	case EntityType::Code: return Style::Code;
	case EntityType::Pre: return Style::Pre;
	case EntityType::Blockquote: return Style::Quote;
	case EntityType::Url:
	case EntityType::Email:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::Mention:
	case EntityType::BotCommand:
	case EntityType::Phone:
	case EntityType::BankCard: return Style::Verbatim;
	default: return std::nullopt;
	}
}

[[nodiscard]] QString MarkdownText(const TextWithEntities &text) {
	auto spans = std::vector<Markdown::Span>();
	for (const auto &entity : text.entities) {
		const auto style = MarkdownStyle(entity.type());
		if (!style) {
			continue;
		}
		auto data = entity.data();
		if (entity.type() == EntityType::MentionName) {
			const auto id = TextUtilities::MentionNameDataToFields(
				data).userId;
			data = id ? (u"tg://user?id="_q + QString::number(id)) : QString();
		}
		spans.push_back({ *style, entity.offset(), entity.length(), data });
	}
	return Markdown::Convert(text.text, spans);
}

[[nodiscard]] bool CanCopyMarkdown(not_null<HistoryItem*> item) {
	return !item->isService()
		&& !item->translatedText().text.isEmpty()
		&& Privacy::AllowsCopy(item->history()->peer)
		&& !Privacy::ForbidsCopy(item);
}

void CopyMarkdown(
		not_null<Window::SessionController*> controller,
		FullMsgId itemId) {
	const auto item = controller->session().data().message(itemId);
	if (!item || !CanCopyMarkdown(item)) {
		controller->showToast(tr::lng_nagram_menu_batch_unavailable(tr::now));
		return;
	}
	QGuiApplication::clipboard()->setText(
		MarkdownText(item->translatedText()));
	controller->showToast(tr::lng_nagram_markdown_copied(tr::now));
}

void Insert(
		not_null<Ui::PopupMenu*> menu,
		int position,
		ActionId id,
		const QString &text,
		const style::icon *icon,
		Fn<void()> callback) {
	const auto target = Destination(menu, id);
	if (target != menu) {
		menu = target;
		position = EndPosition(menu);
	} else {
		position = std::min(position, EndPosition(menu));
	}
	const auto action = Ui::Menu::CreateAction(menu, text, std::move(callback));
	auto widget = base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action, icon, icon);
	Tag(menu->insertAction(position, std::move(widget)), id);
}

} // namespace

void InsertMessageToolActions(
		Ui::PopupMenu *menu,
		HistoryItem *item,
		Window::SessionController *controller,
		Fn<void(HistoryItem*)> select) {
	if (!menu || !controller || !item) {
		return;
	}
	const auto itemId = item->fullId();
	auto position = EndPosition(menu);
	Insert(menu, position++, ActionId::MessageDetails,
		tr::lng_nagram_menu_details(tr::now), &st::menuIconInfo,
		crl::guard(controller, [=] { ShowDetails(controller, itemId); }));
	Insert(menu, position++, ActionId::MessageJson,
		tr::lng_regram_message_json(tr::now), &st::menuIconInfo,
		crl::guard(controller, [=] { ShowMessageJson(controller, itemId); }));
	const auto channel = item->history()->peer->asChannel();
	const auto user = item->from()->asUser();
	if (channel && user && !user->isSelf() && channel->canRestrictParticipant(user)) {
		Insert(menu, position++, ActionId::RestrictSender,
			tr::lng_regram_restrict_sender(tr::now), &st::menuIconPermissions,
			crl::guard(controller, [=] { RestrictSender(controller, itemId); }));
	}
	if (CanSaveToSaved(item)) {
		Insert(menu, position++, ActionId::SaveToSaved,
			tr::lng_nagram_menu_save_to_saved(tr::now),
			&st::menuIconSavedMessages,
			crl::guard(controller, [=] { SaveToSaved(controller, itemId); }));
	}
	if (CanCopyMarkdown(item)) {
		Insert(menu, position++, ActionId::CopyMarkdown,
			tr::lng_nagram_menu_copy_markdown(tr::now), &st::menuIconCopy,
			crl::guard(controller, [=] { CopyMarkdown(controller, itemId); }));
	}
	if (select && item->canBeSelected()) {
		Insert(menu, position, ActionId::SelectAll,
			tr::lng_nagram_menu_select_all(tr::now), &st::menuIconSelect,
			crl::guard(controller, [=] { select(nullptr); }));
	}
}

} // namespace Nagram::Menu
