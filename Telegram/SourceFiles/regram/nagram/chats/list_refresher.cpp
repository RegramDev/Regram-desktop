#include "regram/nagram/chats/list_refresher.h"

#include "regram/nagram/chats/options.h"
#include "regram/filters/runtime.h"
#include "dialogs/dialogs_inner_widget.h"

namespace Nagram {

void ListRefresher::Attach(not_null<Dialogs::InnerWidget*> widget) {
	rpl::merge(
		ForDevice().changes(),
		ForAccount(&widget->session()).changes()
	) | rpl::filter([](std::string_view key) {
		return RegisteredOptions().HasFlag(key, Flag::RefreshDialogList);
	}) | rpl::on_next([=](std::string_view key) {
		if (key == Chats::kCompactList.key || key == Chats::kPreviewLines.key) {
			widget->_geometryInited = false;
			widget->setNarrowRatio(widget->_narrowRatio);
			if (!widget->_filterResults.empty()) {
				widget->refreshFilterResults();
			}
			widget->refreshWithCollapsedRows();
		} else if (key == Chats::kShowArchiveInFolders.key) {
			widget->refreshWithCollapsedRows();
		} else {
			widget->update();
		}
	}, widget->lifetime());
	Regram::Filters::Updates(&widget->session())
		| rpl::on_next([=] { widget->update(); }, widget->lifetime());
}

} // namespace Nagram
