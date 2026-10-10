#pragma once

class DocumentData;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Regram::Filters {

void SettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session);
void ImportDocumentBox(
	not_null<Ui::GenericBox*> box,
	not_null<DocumentData*> document,
	FullMsgId contextId);
void AddRuleAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	QString pattern);

} // namespace Regram::Filters
