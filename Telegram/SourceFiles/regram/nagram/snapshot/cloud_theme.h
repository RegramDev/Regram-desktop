#pragma once

#include "regram/nagram/snapshot/cloud_theme_model.h"
#include "base/weak_ptr.h"

namespace Data {
class DocumentMedia;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
namespace Theme {
struct Instance;
} // namespace Theme
} // namespace Window

namespace Nagram::Snapshot {

enum class CloudThemeState {
	None,
	Loading,
	Ready,
	OwnerUnavailable,
	Missing,
	Failed,
};

class CloudThemeLoader final : public base::has_weak_ptr {
public:
	CloudThemeLoader();
	~CloudThemeLoader();

	void reload();
	void clear();

	[[nodiscard]] CloudThemeState state() const;
	[[nodiscard]] const Window::Theme::Instance *theme() const;
	[[nodiscard]] QString label() const;
	[[nodiscard]] QString note() const;
	[[nodiscard]] rpl::producer<> updates() const;

private:
	void set(CloudThemeState state);
	void release();
	void resolved(const MTPTheme &result);
	void load(not_null<Main::Session*> session);
	void finish();

	CloudThemeState _state = CloudThemeState::None;
	base::weak_ptr<Main::Session> _session;
	CloudThemeRef _ref;
	QString _account;
	mtpRequestId _requestId = 0;
	std::shared_ptr<Data::DocumentMedia> _media;
	std::unique_ptr<Window::Theme::Instance> _theme;
	rpl::event_stream<> _updates;
	rpl::lifetime _loading;

};

void ShowCloudThemePicker(
	not_null<Window::SessionController*> controller,
	Fn<void()> chosen);

} // namespace Nagram::Snapshot
