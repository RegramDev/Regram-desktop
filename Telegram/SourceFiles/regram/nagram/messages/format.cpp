#include "regram/nagram/messages/format.h"

#include "regram/nagram/core/options.h"
#include "regram/nagram/messages/options.h"
#include "regram/nagram/messages/time_format.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/view/history_view_element.h"
#include "ui/painter.h"
#include "ui/text/format_values.h"
#include "ui/text/text_options.h"
#include "lang/lang_keys.h"
#include "base/unixtime.h"
#include "styles/style_chat.h"

#include <QtCore/QLocale>

#include <cstdlib>

namespace Nagram::Messages {

QString FormatTime(QTime time) {
	return FormatTime(time, ForDevice().Get(kSecondsInMessages));
}

QString FormatSavedFrom(QDateTime dateTime) {
	if (!ForDevice().Get(kSecondsInMessages)) {
		return Ui::FormatDateTimeSavedFrom(dateTime);
	}
	const auto current = QDate::currentDate();
	const auto date = dateTime.date();
	const auto time = FormatTime(dateTime.time());
	if (date == current) {
		return tr::lng_mediaview_today(tr::now, lt_time, time);
	} else if (date == current.addDays(-1)) {
		return tr::lng_mediaview_yesterday(tr::now, lt_time, time);
	}
	constexpr auto kSecondsInYear = 365 * 24 * 60 * 60;
	const auto diff = std::abs(
		base::unixtime::now() - base::unixtime::serialize(dateTime));
	const auto dateText = (diff < kSecondsInYear)
		? tr::lng_month_day(
			tr::now,
			lt_month,
			Lang::MonthSmall(date.month())(tr::now),
			lt_day,
			QString::number(date.day()))
		: langDayOfMonthFull(date);
	return tr::lng_mediaview_date_time(
		tr::now, lt_date, dateText, lt_time, time);
}

namespace {

QString ActiveEditedMark() {
	return ForDevice().Get(kEditedIcon)
		? QString(QChar(0x270E))
		: ForDevice().Get(kEditedMark);
}

} // namespace

QString FormatEditedDate(QDateTime sent, QDateTime edited) {
	const auto today = QDateTime::currentDateTime().date();
	const auto time = FormatTime(edited.time());
	const auto mark = ActiveEditedMark();
	if (sent.date() == today && edited.date() == today) {
		return mark.isEmpty()
			? tr::lng_edited_at(tr::now, lt_time, time)
			: mark + ' ' + time;
	}
	const auto date = langDayOfMonthShort(edited.date());
	return mark.isEmpty()
		? tr::lng_edited_on(tr::now, lt_date, date, lt_time, time)
		: mark + ' ' + date + ' ' + time;
}

QString EditedMark() {
	const auto mark = ActiveEditedMark();
	return mark.isEmpty() ? tr::lng_edited(tr::now) : mark;
}

QString FormatCounter(int count) {
	return ForDevice().Get(kExactMessageCounters)
		? Lang::FormatCountDecimal(count)
		: Lang::FormatCountToShort(count).string;
}

void ApplyInfoOptions(HistoryView::BottomInfo::Data &data) {
	auto &options = ForDevice();
	if (options.Get(kHideMessageViews)) {
		data.views.reset();
	}
	if (options.Get(kHideChannelSignature)) {
		data.author.clear();
	}
	if (options.Get(kHideEditedBadge)) {
		using Flag = HistoryView::BottomInfo::Data::Flag;
		data.flags &= ~(Flag::Edited | Flag::EditedPrimary);
	}
}

void LayoutForwards(
		Ui::Text::String &text,
		const HistoryView::BottomInfo::Data &data) {
	using Flag = HistoryView::BottomInfo::Data::Flag;
	if (!data.views
		|| !data.forwardsCount
		|| (data.flags & Flag::Sending)
		|| !ForDevice().Get(kShowForwardCount)) {
		text.clear();
		return;
	}
	text.setText(
		st::msgDateTextStyle,
		FormatCounter(*data.forwardsCount),
		Ui::NameTextOptions());
}

int ForwardsWidth(const Ui::Text::String &text) {
	return text.isEmpty()
		? 0
		: (st::historyViewsSpace + text.maxWidth() + st::historyViewsWidth);
}

void PaintForwards(
		Painter &p,
		const Ui::Text::String &text,
		const style::icon &icon,
		int &right,
		int top,
		int outerWidth) {
	if (text.isEmpty()) {
		return;
	}
	const auto width = text.maxWidth();
	right -= st::historyViewsSpace + width;
	text.drawLeft(p, right, top, width, outerWidth);
	right -= st::historyViewsWidth;
	const auto left = style::RightToLeft()
		? (outerWidth - right - icon.width())
		: right;
	// The mirrored reply arrow already has the colors of this bubble.
	p.save();
	p.translate(
		left + icon.width(),
		top + st::msgDateFont->height + st::historyViewsTop);
	p.scale(-1., 1.);
	icon.paint(p, 0, 0, icon.width());
	p.restore();
}

void ApplyForwardedDate(
		HistoryView::BottomInfo::Data &data,
		not_null<HistoryItem*> item) {
	if (!ForDevice().Get(kShowForwardedMessageDate)
		|| item->externalReply()) {
		return;
	}
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	if (!forwarded || !forwarded->originalDate) {
		return;
	}
	data.date = base::unixtime::parse(forwarded->originalDate);
	data.flags |= HistoryView::BottomInfo::Data::Flag::ForwardedDate;
}

TextWithEntities ServiceText(
		not_null<HistoryView::Element*> view,
		const TextWithEntities &text) {
	if (!ForDevice().Get(kShowServiceTime)
		|| text.empty()
		|| view->data()->date() <= 0) {
		return text;
	}
	auto result = text;
	result.text += u" · "_q + FormatTime(view->dateTime().time());
	return result;
}

QString WithMessageId(QString text, not_null<HistoryItem*> item) {
	if (ForDevice().Get(kShowMessageId)
		&& IsServerMsgId(item->id)
		&& !item->isSending()) {
		text += '\n' + tr::lng_nagram_message_id(
			tr::now, lt_id, QString::number(item->id.bare));
	}
	return text;
}

} // namespace Nagram::Messages
