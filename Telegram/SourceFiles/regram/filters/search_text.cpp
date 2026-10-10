#include "regram/filters/search_text.h"

#include "regram/filters/rules.h"
#include "data/data_groups.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "iv/iv_rich_page.h"

namespace Regram::Filters {
namespace {

constexpr auto kMaxDepth = 64;
constexpr auto kMaxBlocks = 4096;

class Collector final {
public:
	void add(const QString &text);
	void add(const TextWithEntities &text);
	void blocks(const std::vector<Iv::RichPage::Block> &blocks, int depth);
	[[nodiscard]] QString result() const;
	[[nodiscard]] bool advance();

private:
	QString _text;
	int _blocks = 0;
	bool _limited = false;

};

bool Collector::advance() {
	_limited = _limited || ++_blocks > kMaxBlocks;
	return !_limited;
}

void Collector::add(const QString &text) {
	if (text.isEmpty() || _limited) {
		return;
	}
	const auto available = kMaxTextUnits - _text.size();
	if (text.size() + (_text.isEmpty() ? 0 : 1) > available) {
		_limited = true;
		return;
	}
	if (!_text.isEmpty()) {
		_text += u'\n';
	}
	_text += text;
}

void Collector::add(const TextWithEntities &text) {
	add(text.text);
	for (const auto &entity : text.entities) {
		if (entity.type() == EntityType::CustomUrl) {
			add(entity.data());
		}
	}
}

void Collector::blocks(const std::vector<Iv::RichPage::Block> &blocks, int depth) {
	if (depth > kMaxDepth) {
		_limited = true;
		return;
	}
	for (const auto &block : blocks) {
		if (!advance()) {
			return;
		}
		add(block.text.text);
		add(block.caption.text);
		add(block.formula);
		add(block.url);
		for (const auto &row : block.tableRows) {
			for (const auto &cell : row.cells) {
				if (!advance()) {
					return;
				}
				add(cell.text.text);
			}
		}
		for (const auto &button : block.buttons) {
			if (!advance()) {
				return;
			}
			add(button.text.text);
			add(button.button.text);
		}
		for (const auto &entry : block.listItems) {
			if (!advance()) {
				return;
			}
			add(entry.text.text);
			this->blocks(entry.blocks, depth + 1);
		}
		this->blocks(block.blocks, depth + 1);
	}
}

QString Collector::result() const {
	return _limited ? QString(kMaxTextUnits + 1, u' ') : _text;
}

} // namespace

QString SearchText(not_null<HistoryItem*> item) {
	auto collector = Collector();
	const auto add = [&](not_null<HistoryItem*> part) {
		if (const auto page = part->richPage()) {
			collector.blocks(page->blocks, 0);
		} else {
			collector.add(part->originalText());
		}
		if (const auto markup = part->Get<HistoryMessageReplyMarkup>()) {
			for (const auto &row : markup->data.rows) {
				for (const auto &button : row) {
					if (!collector.advance()) {
						return;
					}
					collector.add(button.text);
					collector.add(QString::fromUtf8(button.data));
				}
			}
		}
	};
	if (const auto group = item->history()->owner().groups().find(item)) {
		for (const auto &part : group->items) {
			add(part);
		}
	} else {
		add(item);
	}
	return collector.result();
}

} // namespace Regram::Filters
