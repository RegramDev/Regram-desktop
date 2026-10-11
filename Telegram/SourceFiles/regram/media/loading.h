#pragma once

#include "data/data_msg_id.h"

#include <gsl/pointers>
#include <vector>

class QWidget;

namespace Main {
class Session;
} // namespace Main

namespace HistoryView {
class Element;
class ElementDelegate;
} // namespace HistoryView

namespace Regram::Media {

void UpdateViewport(
	gsl::not_null<Main::Session*> session,
	gsl::not_null<HistoryView::ElementDelegate*> delegate,
	gsl::not_null<QWidget*> widget,
	int top,
	int bottom);
void PrefetchWindow(
	gsl::not_null<Main::Session*> session,
	gsl::not_null<HistoryView::ElementDelegate*> delegate,
	std::vector<FullMsgId> messages);
[[nodiscard]] bool AdmitInline(gsl::not_null<HistoryView::Element*> view);
void ResetPrefetch(gsl::not_null<Main::Session*> session);
void ForgetInline(gsl::not_null<HistoryView::Element*> view);

} // namespace Regram::Media
