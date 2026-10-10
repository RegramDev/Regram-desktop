#include "regram/nagram/services/context.h"

#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "regram/nagram/filters/view.h"
#include "regram/nagram/services/context_model.h"

namespace Nagram {
namespace {

constexpr auto kContextScan = 64;

} // namespace

QStringList CollectTranslationContext(not_null<HistoryItem*> item) {
	auto loaded = std::vector<not_null<HistoryItem*>>();
	for (const auto &block : item->history()->blocks) {
		for (const auto &view : block->messages) {
			if (view->data() != item) {
				loaded.push_back(view->data());
				continue;
			}
			const auto skip = std::max(int(loaded.size()) - kContextScan, 0);
			auto before = std::vector<ContextCandidate>();
			for (const auto &other : loaded | ranges::views::drop(skip)) {
				before.push_back({
					.text = other->originalText().text,
					.topic = other->topicRootId().bare,
					.regular = other->isRegular() && !other->isService(),
					.hidden = Filters::Hidden(other),
					.restricted = other->forbidsForward(),
				});
			}
			return SelectContext(
				before,
				item->topicRootId().bare,
				item->forbidsForward());
		}
	}
	return {};
}

} // namespace Nagram
