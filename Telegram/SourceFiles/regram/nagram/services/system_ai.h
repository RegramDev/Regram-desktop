#pragma once

namespace Ui {
class Show;
} // namespace Ui

namespace HistoryView::Controls {
struct ComposeAiBoxArgs;
} // namespace HistoryView::Controls

namespace Nagram {

[[nodiscard]] int SystemAiAvailability();
[[nodiscard]] QString SystemAiStatusText(int status);
void ShowSystemAi(
	std::shared_ptr<Ui::Show> show,
	HistoryView::Controls::ComposeAiBoxArgs &&args);

} // namespace Nagram
