#pragma once

class PeerData;
class UserData;
class QSize;
class QString;
struct ClickContext;

namespace Dialogs {
struct SearchState;
} // namespace Dialogs

namespace Nagram::Links {

[[nodiscard]] bool AutoLoginDisabled();

class HashtagClickScope final {
public:
	HashtagClickScope(const ClickContext &context, const QString &tag);
	~HashtagClickScope();

private:
	const bool _wasActive = false;
	PeerData * const _wasPeer = nullptr;

};

void ApplyHashtagSearchPage(Dialogs::SearchState &state);

[[nodiscard]] QSize WebAppPanelSize(QSize base);
[[nodiscard]] const char *WebAppPlatform(not_null<UserData*> bot);
[[nodiscard]] bool OpenOutsideWebview(
	const QString &uri,
	const QString &startUrl);

} // namespace Nagram::Links
