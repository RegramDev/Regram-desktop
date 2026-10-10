#pragma once

class QWidget;
namespace Ui {
class InputField;
} // namespace Ui

namespace Nagram::Compose {

void SetupFormatToolbar(
	not_null<Ui::InputField*> field,
	not_null<QWidget*> parent);

} // namespace Nagram::Compose
