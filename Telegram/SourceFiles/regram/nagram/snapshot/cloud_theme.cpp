#include "regram/nagram/snapshot/cloud_theme.h"

#include "apiwrap.h"
#include "core/application.h"
#include "data/data_cloud_themes.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "storage/file_download.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram::Snapshot {
namespace {

[[nodiscard]] std::vector<not_null<Main::Session*>> Sessions() {
	auto result = std::vector<not_null<Main::Session*>>();
	for (const auto &account : Core::App().domain().orderedAccounts()) {
		if (account->sessionExists()) {
			result.push_back(&account->session());
		}
	}
	return result;
}

[[nodiscard]] QString AccountKey(not_null<Main::Session*> session) {
	return QString::number(session->uniqueId());
}

[[nodiscard]] Main::Session *FindSession(const QString &key) {
	for (const auto &session : Sessions()) {
		if (AccountKey(session) == key) {
			return session;
		}
	}
	return nullptr;
}

[[nodiscard]] CloudThemeRef RefFor(
		not_null<Main::Session*> session,
		const Data::CloudTheme &theme) {
	return {
		.user = session->userId().bare,
		.themeId = theme.id,
		.accessHash = theme.accessHash,
		.documentId = theme.documentId,
		.title = theme.title.simplified().left(kCloudThemeTitleLimit),
		.slug = theme.slug.simplified(),
	};
}

void Forget() {
	const auto previous = ForDevice().Get(kCloudAccount);
	if (const auto session = FindSession(previous)) {
		Expects(ForAccount(session).Set(kCloudTheme, QByteArray()));
	}
	Expects(ForDevice().Set(kCloudAccount, QString()));
}

void Choose(
		not_null<Main::Session*> session,
		const Data::CloudTheme &theme) {
	const auto raw = SerializeCloudThemeRef(RefFor(session, theme));
	if (!ValidCloudThemeRef(raw)) {
		return;
	}
	Forget();
	Expects(ForAccount(session).Set(kCloudTheme, raw));
	Expects(ForDevice().Set(kCloudAccount, AccountKey(session)));
}

} // namespace

CloudThemeLoader::CloudThemeLoader() = default;

CloudThemeLoader::~CloudThemeLoader() {
	release();
}

void CloudThemeLoader::release() {
	if (const auto session = _session.get()) {
		session->api().request(base::take(_requestId)).cancel();
	}
	_requestId = 0;
	_loading.destroy();
	_media = nullptr;
	_session = nullptr;
}

void CloudThemeLoader::set(CloudThemeState state) {
	_state = state;
	_updates.fire({});
}

void CloudThemeLoader::clear() {
	release();
	_theme = nullptr;
	Forget();
	set(CloudThemeState::None);
}

void CloudThemeLoader::reload() {
	release();
	_theme = nullptr;
	_ref = CloudThemeRef();
	_account = QString();
	const auto key = ForDevice().Get(kCloudAccount);
	if (key.isEmpty()) {
		set(CloudThemeState::None);
		return;
	}
	const auto session = FindSession(key);
	if (!session) {
		set(CloudThemeState::OwnerUnavailable);
		return;
	}
	const auto ref = ParseCloudThemeRef(ForAccount(session).Get(kCloudTheme));
	if (!ref || ref->user != session->userId().bare) {
		Forget();
		set(CloudThemeState::Missing);
		return;
	}
	_session = base::make_weak(session);
	_ref = *ref;
	if (Core::App().domain().accountsAuthedCount() > 1) {
		_account = session->user()->name();
	}
	session->account().sessionChanges(
	) | rpl::on_next([=] {
		release();
		_theme = nullptr;
		set(CloudThemeState::OwnerUnavailable);
	}, _loading);
	_requestId = session->api().request(MTPaccount_GetTheme(
		MTP_string(Data::CloudThemes::Format()),
		MTP_inputTheme(MTP_long(_ref.themeId), MTP_long(_ref.accessHash))
	)).done(crl::guard(this, [=](const MTPTheme &result) {
		_requestId = 0;
		resolved(result);
	})).fail(crl::guard(this, [=](const MTP::Error &error) {
		_requestId = 0;
		if (error.type() == u"THEME_INVALID"_q) {
			release();
			Forget();
			set(CloudThemeState::Missing);
		} else {
			set(CloudThemeState::Failed);
		}
	})).send();
	set(CloudThemeState::Loading);
}

void CloudThemeLoader::resolved(const MTPTheme &result) {
	const auto session = _session.get();
	if (!session) {
		return;
	}
	const auto theme = Data::CloudTheme::Parse(session, result);
	if (theme.id != _ref.themeId || !theme.documentId) {
		set(CloudThemeState::Failed);
		return;
	}
	const auto updated = RefFor(session, theme);
	if (updated != _ref) {
		const auto raw = SerializeCloudThemeRef(updated);
		if (ValidCloudThemeRef(raw)) {
			_ref = updated;
			Expects(ForAccount(session).Set(kCloudTheme, raw));
		}
	}
	load(session);
}

void CloudThemeLoader::load(not_null<Main::Session*> session) {
	const auto document = session->data().document(_ref.documentId);
	if (document->size > Storage::kMaxFileInMemory) {
		set(CloudThemeState::Failed);
		return;
	}
	_media = document->createMediaView();
	document->save(
		Data::FileOriginTheme(_ref.themeId, _ref.accessHash),
		QString());
	if (_media->loaded()) {
		finish();
		return;
	}
	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		if (_media->loaded()) {
			finish();
		} else if (!document->loading()) {
			release();
			set(CloudThemeState::Failed);
		}
	}, _loading);
}

void CloudThemeLoader::finish() {
	const auto bytes = _media->bytes();
	release();
	auto theme = std::make_unique<Window::Theme::Instance>();
	if (bytes.isEmpty()
		|| !Window::Theme::LoadFromContent(bytes, theme.get(), nullptr)) {
		set(CloudThemeState::Failed);
		return;
	}
	_theme = std::move(theme);
	set(CloudThemeState::Ready);
}

CloudThemeState CloudThemeLoader::state() const {
	return _state;
}

const Window::Theme::Instance *CloudThemeLoader::theme() const {
	return (_state == CloudThemeState::Ready) ? _theme.get() : nullptr;
}

QString CloudThemeLoader::label() const {
	if (_state == CloudThemeState::None
		|| _state == CloudThemeState::Missing) {
		return tr::lng_nagram_snapshot_cloud_theme_none(tr::now);
	} else if (_ref.title.isEmpty()) {
		return tr::lng_nagram_snapshot_cloud_theme_other(tr::now);
	}
	return _account.isEmpty()
		? _ref.title
		: tr::lng_nagram_snapshot_cloud_theme_account(
			tr::now,
			lt_title,
			_ref.title,
			lt_name,
			_account);
}

QString CloudThemeLoader::note() const {
	switch (_state) {
	case CloudThemeState::Loading:
		return tr::lng_nagram_snapshot_cloud_theme_loading(tr::now);
	case CloudThemeState::OwnerUnavailable:
		return tr::lng_nagram_snapshot_cloud_theme_unavailable(tr::now);
	case CloudThemeState::Missing:
		return tr::lng_nagram_snapshot_cloud_theme_missing(tr::now);
	case CloudThemeState::Failed:
		return tr::lng_nagram_snapshot_cloud_theme_error(tr::now);
	case CloudThemeState::None:
	case CloudThemeState::Ready:
		return QString();
	}
	Unexpected("Cloud theme state.");
}

rpl::producer<> CloudThemeLoader::updates() const {
	return _updates.events();
}

void ShowCloudThemePicker(
		not_null<Window::SessionController*> controller,
		Fn<void()> chosen) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_snapshot_cloud_theme_choose());
		const auto list = box->verticalLayout()->add(
			object_ptr<Ui::VerticalLayout>(box));
		auto sessions = std::vector<base::weak_ptr<Main::Session>>();
		for (const auto &session : Sessions()) {
			sessions.push_back(base::make_weak(session));
		}
		const auto rebuild = [=] {
			list->clear();
			auto found = false;
			for (const auto &weak : sessions) {
				const auto session = weak.get();
				if (!session) {
					continue;
				}
				auto titled = (sessions.size() < 2);
				for (const auto &theme : session->data().cloudThemes().list()) {
					if (!theme.documentId) {
						continue;
					}
					if (!titled) {
						titled = true;
						Ui::AddSubsectionTitle(
							list,
							rpl::single(session->user()->name()));
					}
					found = true;
					const auto button = list->add(
						object_ptr<Ui::SettingsButton>(
							list,
							rpl::single(theme.title.isEmpty()
								? theme.slug
								: theme.title),
							st::settingsButtonNoIcon));
					button->setClickedCallback([=] {
						if (const auto session = weak.get()) {
							Choose(session, theme);
							box->closeBox();
							chosen();
						}
					});
				}
			}
			if (!found) {
				list->add(
					object_ptr<Ui::FlatLabel>(
						list,
						tr::lng_nagram_snapshot_cloud_theme_empty(),
						st::boxLabel),
					st::boxRowPadding);
			}
			list->resizeToWidth(box->width());
		};
		for (const auto &session : Sessions()) {
			session->data().cloudThemes().updated(
			) | rpl::on_next(rebuild, box->lifetime());
			session->data().cloudThemes().refresh();
		}
		rebuild();
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram::Snapshot
