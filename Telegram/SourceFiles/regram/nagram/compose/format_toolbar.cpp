#include "regram/nagram/compose/format_toolbar.h"

#include "regram/nagram/compose/options.h"
#include "base/unique_qptr.h"
#include "lang/lang_keys.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "styles/style_nagram_compose.h"
#include "styles/style_widgets.h"

#include <QtWidgets/QTextEdit>

namespace Nagram::Compose {
namespace {

enum class Action { Tag, Link, Clear, Newline };

struct Entry {
	Action action = Action::Tag;
	const QString *tag = nullptr;
	const style::icon *icon = nullptr;
	const style::icon *active = nullptr;
	const tr::phrase<> *name = nullptr;
};

std::vector<Entry> Entries() {
	using Field = Ui::InputField;
	return {
		{ Action::Tag, &Field::kTagBold, &st::nagramFormatBold,
			&st::nagramFormatBoldActive, &tr::lng_menu_formatting_bold },
		{ Action::Tag, &Field::kTagItalic, &st::nagramFormatItalic,
			&st::nagramFormatItalicActive, &tr::lng_menu_formatting_italic },
		{ Action::Tag, &Field::kTagUnderline, &st::nagramFormatUnderline,
			&st::nagramFormatUnderlineActive,
			&tr::lng_menu_formatting_underline },
		{ Action::Tag, &Field::kTagStrikeOut, &st::nagramFormatStrikeOut,
			&st::nagramFormatStrikeOutActive,
			&tr::lng_menu_formatting_strike_out },
		{ Action::Tag, &Field::kTagCode, &st::nagramFormatCode,
			&st::nagramFormatCodeActive, &tr::lng_menu_formatting_monospace },
		{ Action::Tag, &Field::kTagPre, &st::nagramFormatCode,
			&st::nagramFormatCodeActive, &tr::lng_regram_format_code_block },
		{ Action::Tag, &Field::kTagSpoiler, &st::nagramFormatSpoiler,
			&st::nagramFormatSpoilerActive,
			&tr::lng_menu_formatting_spoiler },
		{ Action::Tag, &Field::kTagBlockquote, &st::nagramFormatQuote,
			&st::nagramFormatQuoteActive,
			&tr::lng_menu_formatting_blockquote },
		{ Action::Link, nullptr, &st::nagramFormatLink, nullptr,
			&tr::lng_menu_formatting_link_create },
		{ Action::Clear, nullptr, &st::nagramFormatClear, nullptr,
			&tr::lng_menu_formatting_clear },
		{ Action::Newline, nullptr, &st::nagramMenuOrderDown, nullptr,
			&tr::lng_regram_format_newline },
	};
}

class FormatToolbar final : public Ui::RpWidget {
public:
	FormatToolbar(
		not_null<QWidget*> parent,
		not_null<Ui::InputField*> field);

private:
	void paintEvent(QPaintEvent *e) override;
	void apply(const Entry &entry);
	void refresh();
	void refreshActive();

	const not_null<Ui::InputField*> _field;
	std::vector<std::pair<not_null<Ui::IconButton*>, Entry>> _buttons;
	bool _enabled = false;

};

FormatToolbar::FormatToolbar(
	not_null<QWidget*> parent,
	not_null<Ui::InputField*> field)
: RpWidget(parent)
, _field(field) {
	const auto &padding = st::nagramFormatPadding;
	auto left = padding.left();
	for (const auto &entry : Entries()) {
		const auto button = Ui::CreateChild<Ui::IconButton>(
			this,
			st::nagramFormatButton);
		button->setIconOverride(entry.icon, entry.icon);
		button->setFocusPolicy(Qt::NoFocus);
		(*entry.name)() | rpl::on_next([=](const QString &name) {
			button->setAccessibleName(name);
			button->setToolTip(name);
		}, button->lifetime());
		button->moveToLeft(left, padding.top());
		button->setClickedCallback([=] { apply(entry); });
		left += button->width();
		_buttons.emplace_back(button, entry);
	}
	resize(
		left + padding.right(),
		padding.top() + st::nagramFormatButtonSize + padding.bottom());
	hide();

	QObject::connect(
		field->rawTextEdit(),
		&QTextEdit::selectionChanged,
		this,
		[=] { refresh(); });
	rpl::merge(
		field->focusedChanges() | rpl::to_empty,
		field->changes(),
		field->heightChanges(),
		field->geometryValue() | rpl::to_empty,
		field->shownValue() | rpl::to_empty
	) | rpl::on_next([=] {
		refresh();
	}, lifetime());
	ForDevice().Value(kFormatToolbar) | rpl::on_next([=](bool enabled) {
		_enabled = enabled;
		refresh();
	}, lifetime());
}

void FormatToolbar::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	auto hq = PainterHighQualityEnabler(p);
	auto pen = QPen(st::menuSeparatorFg->c);
	pen.setWidth(st::lineWidth);
	p.setPen(pen);
	p.setBrush(st::menuBg);
	const auto radius = st::nagramFormatRadius;
	const auto inset = st::lineWidth / 2.;
	p.drawRoundedRect(QRectF(rect()).marginsRemoved({ inset, inset, inset, inset }),
		radius,
		radius);
}

void FormatToolbar::apply(const Entry &entry) {
	switch (entry.action) {
	case Action::Tag:
		_field->toggleCurrentMarkdownTag(*entry.tag);
		break;
	case Action::Link:
		_field->editCurrentMarkdownLink();
		break;
	case Action::Clear:
		_field->clearCurrentMarkdown();
		break;
	case Action::Newline: {
		auto cursor = _field->textCursor();
		cursor.setPosition(cursor.selectionEnd());
		cursor.insertBlock();
		_field->setTextCursor(cursor);
	} break;
	}
	refresh();
}

void FormatToolbar::refreshActive() {
	for (const auto &[button, entry] : _buttons) {
		const auto icon = (entry.tag && entry.active
			&& _field->isMarkdownTagActive(*entry.tag))
			? entry.active
			: entry.icon;
		button->setIconOverride(icon, icon);
	}
}

void FormatToolbar::refresh() {
	const auto cursor = _field->textCursor();
	const auto parent = parentWidget();
	if (!_enabled
		|| !parent
		|| _field->isHidden()
		|| !_field->hasFocus()
		|| !cursor.hasSelection()) {
		hide();
		return;
	}
	const auto &padding = st::nagramFormatPadding;
	const auto size = st::nagramFormatButtonSize;
	const auto columns = std::clamp(
		(parent->width() - padding.left() - padding.right()) / size,
		1,
		int(_buttons.size()));
	resize(
		padding.left() + columns * size + padding.right(),
		padding.top() + ((int(_buttons.size()) + columns - 1) / columns) * size
			+ padding.bottom());
	for (auto index = 0; index != int(_buttons.size()); ++index) {
		_buttons[index].first->moveToLeft(
			padding.left() + (index % columns) * size,
			padding.top() + (index / columns) * size);
	}
	refreshActive();
	const auto edit = _field->rawTextEdit();
	auto start = cursor;
	start.setPosition(cursor.selectionStart());
	const auto caret = edit->cursorRect(start);
	const auto anchor = edit->viewport()->mapTo(parent, caret.topLeft());
	const auto fieldTop = _field->mapTo(parent, QPoint()).y();
	const auto skip = st::nagramFormatSkip;
	auto top = std::min(fieldTop, anchor.y()) - height() - skip;
	if (top < 0) {
		top = anchor.y() + caret.height() + skip;
	}
	top = std::clamp(top, 0, std::max(parent->height() - height(), 0));
	const auto maxLeft = std::max(parent->width() - width(), 0);
	const auto left = std::clamp(anchor.x() - width() / 2, 0, maxLeft);
	move(left, top);
	raise();
	show();
}

} // namespace

void SetupFormatToolbar(
		not_null<Ui::InputField*> field,
		not_null<QWidget*> parent) {
	field->lifetime().make_state<base::unique_qptr<FormatToolbar>>(
		base::make_unique_q<FormatToolbar>(parent, field));
}

} // namespace Nagram::Compose
