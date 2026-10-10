#pragma once

#include "regram/nagram/core/options.h"
#include "regram/nagram/snapshot/cloud_theme_model.h"

#include <QtCore/QJsonObject>
#include <QtGui/QImage>

class HistoryItem;
namespace Ui { class PopupMenu; }
namespace Window {
class SessionController;
namespace Theme {
struct Instance;
} // namespace Theme
} // namespace Window

namespace Nagram::Snapshot {

[[nodiscard]] QJsonObject Defaults();
[[nodiscard]] bool Valid(const QJsonObject &value);
[[nodiscard]] bool Validate(const QByteArray &raw);
[[nodiscard]] std::variant<QImage, QString> Render(
	not_null<Window::SessionController*> controller,
	const MessageIdsList &ids,
	const QJsonObject &options,
	bool revealSpoilers,
	const Window::Theme::Instance *cloud = nullptr);
void InsertAction(
	Ui::PopupMenu *menu,
	Window::SessionController *controller,
	HistoryItem *item,
	MessageIdsList selected);

inline const auto kSettings = Option<QByteArray>{
	"nagram.snapshot", Scope::Device, QByteArray(),
	Category::Menu, "lng_nagram_snapshot", 0, Validate };

inline void RegisterOptions(Registry &registry) {
	Expects(registry.Add(kSettings));
	RegisterCloudThemeOptions(registry);
}

} // namespace Nagram::Snapshot
