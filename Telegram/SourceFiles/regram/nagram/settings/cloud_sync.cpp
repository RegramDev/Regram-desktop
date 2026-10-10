#include "regram/nagram/settings/cloud_sync.h"

#include "regram/nagram/settings/config.h"
#include "regram/nagram/sync/service.h"

#include "base/unixtime.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"

namespace Nagram {
namespace {

using Sync::Action;
using Sync::Checked;
using Sync::Error;
using Sync::Service;

struct Progress {
	base::weak_ptr<Service> service;
	base::weak_qptr<Ui::GenericBox> box;
	rpl::variable<QString> text;
	bool finished = false;
};

[[nodiscard]] QString ErrorText(Error error) {
	switch (error) {
	case Error::None: return QString();
	case Error::Busy: return tr::lng_nagram_sync_busy(tr::now);
	case Error::LocalInvalid: return tr::lng_nagram_config_invalid(tr::now);
	case Error::Search:
	case Error::Download: return tr::lng_nagram_sync_download_error(tr::now);
	case Error::TooLarge: return tr::lng_nagram_sync_too_large(tr::now);
	case Error::Upload: return tr::lng_nagram_sync_upload_error(tr::now);
	case Error::Damaged: return tr::lng_nagram_sync_invalid(tr::now);
	case Error::Foreign: return tr::lng_nagram_sync_foreign(tr::now);
	case Error::Newer: return tr::lng_nagram_sync_newer_format(tr::now);
	case Error::Timeout: return tr::lng_nagram_sync_timeout(tr::now);
	case Error::RemoteChanged:
		return tr::lng_nagram_sync_remote_newer(tr::now);
	}
	Unexpected("Sync error.");
}

[[nodiscard]] std::shared_ptr<Progress> ShowProgress(
		not_null<Window::SessionController*> controller) {
	const auto progress = std::make_shared<Progress>();
	progress->service = base::make_weak(
		&Service::For(&controller->session()));
	progress->text = tr::lng_nagram_sync_checking(tr::now);
	auto box = Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_sync_title());
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			progress->text.value(),
			st::boxLabel));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		box->lifetime().add([=] {
			const auto service = progress->service.get();
			if (service && !base::take(progress->finished)) {
				service->cancel();
			}
		});
	});
	progress->box = box.data();
	controller->show(std::move(box));
	return progress;
}

void Finish(
		not_null<Window::SessionController*> controller,
		const std::shared_ptr<Progress> &progress,
		const QString &toast) {
	progress->finished = true;
	if (const auto box = progress->box.get()) {
		box->closeBox();
	}
	if (!toast.isEmpty()) {
		controller->showToast(toast);
	}
}

void Upload(
		not_null<Window::SessionController*> controller,
		const std::shared_ptr<Progress> &progress) {
	const auto service = progress->service.get();
	if (!service) {
		return;
	}
	progress->text = tr::lng_nagram_sync_uploading(tr::now);
	service->upload(crl::guard(controller, [=](Error error) {
		Finish(controller, progress, (error == Error::None)
			? tr::lng_nagram_sync_done(tr::now)
			: ErrorText(error));
	}));
}

void Preview(
		not_null<Window::SessionController*> controller,
		const Sync::Envelope &remote) {
	const auto session = base::make_weak(&controller->session());
	ShowImport(controller, remote.payload, ExchangeTarget::Sync, [=] {
		if (const auto strong = session.get()) {
			Service::For(strong).applied(remote);
		}
	});
}

[[nodiscard]] bool Unreadable(const Checked &checked) {
	return checked.found
		&& (checked.error == Error::TooLarge
			|| checked.error == Error::Damaged
			|| checked.error == Error::Foreign
			|| checked.error == Error::Newer);
}

void Check(
		not_null<Window::SessionController*> controller,
		bool replacing,
		Fn<void(std::shared_ptr<Progress>, Checked)> done) {
	auto &service = Service::For(&controller->session());
	if (service.busy()) {
		controller->showToast(ErrorText(Error::Busy));
		return;
	}
	const auto progress = ShowProgress(controller);
	service.check(crl::guard(controller, [=](Checked checked) {
		if (checked.error != Error::None
			&& !(replacing && Unreadable(checked))) {
			Finish(controller, progress, ErrorText(checked.error));
		} else {
			done(progress, std::move(checked));
		}
	}));
}

} // namespace

void BackupToCloud(not_null<Window::SessionController*> controller) {
	Check(controller, true, [=](
			std::shared_ptr<Progress> progress,
			Checked checked) {
		const auto unreadable = Unreadable(checked);
		if (!unreadable
			&& (!checked.found || checked.action == Action::Upload)) {
			Upload(controller, progress);
			return;
		} else if (!unreadable && checked.action == Action::UpToDate) {
			Finish(
				controller,
				progress,
				tr::lng_nagram_sync_up_to_date(tr::now));
			return;
		}
		Finish(controller, progress, QString());
		const auto question = tr::lng_nagram_sync_overwrite_confirm(tr::now);
		controller->show(Ui::MakeConfirmBox({
			.text = (unreadable
				? (ErrorText(checked.error) + u"\n\n"_q + question)
				: question),
			.confirmed = crl::guard(controller, [=](Fn<void()> close) {
				close();
				Upload(controller, ShowProgress(controller));
			}),
			.confirmText = tr::lng_nagram_sync_use_local(),
		}));
	});
}

void RestoreFromCloud(not_null<Window::SessionController*> controller) {
	Check(controller, false, [=](
			std::shared_ptr<Progress> progress,
			Checked checked) {
		if (!checked.found) {
			Finish(
				controller,
				progress,
				tr::lng_nagram_sync_not_found(tr::now));
		} else if (checked.action == Action::UpToDate) {
			Finish(
				controller,
				progress,
				tr::lng_nagram_sync_up_to_date(tr::now));
		} else {
			Finish(controller, progress, QString());
			Preview(controller, checked.remote);
		}
	});
}

void SyncWithCloud(not_null<Window::SessionController*> controller) {
	Check(controller, false, [=](
			std::shared_ptr<Progress> progress,
			Checked checked) {
		if (!checked.found || checked.action == Action::Upload) {
			Upload(controller, progress);
		} else if (checked.action == Action::UpToDate) {
			Finish(
				controller,
				progress,
				tr::lng_nagram_sync_up_to_date(tr::now));
		} else if (checked.action == Action::Download) {
			Finish(controller, progress, QString());
			Preview(controller, checked.remote);
		} else {
			Finish(controller, progress, QString());
			const auto remote = checked.remote;
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_nagram_sync_now());
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					tr::lng_nagram_sync_both_changed(),
					st::boxLabel));
				box->addButton(
					tr::lng_nagram_sync_use_cloud(),
					crl::guard(controller, [=] {
						box->closeBox();
						Preview(controller, remote);
					}));
				box->addButton(
					tr::lng_nagram_sync_use_local(),
					crl::guard(controller, [=] {
						box->closeBox();
						Upload(controller, ShowProgress(controller));
					}));
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
			}));
		}
	});
}

void DeleteCloudBackup(not_null<Window::SessionController*> controller) {
	controller->show(Ui::MakeConfirmBox({
		.text = tr::lng_nagram_sync_delete_remote_confirm(),
		.confirmed = crl::guard(controller, [=](Fn<void()> close) {
			close();
			auto &service = Service::For(&controller->session());
			if (service.busy()) {
				controller->showToast(ErrorText(Error::Busy));
				return;
			}
			const auto progress = ShowProgress(controller);
			service.removeRemote(crl::guard(controller, [=](Error error) {
				Finish(controller, progress, (error == Error::None)
					? tr::lng_nagram_sync_deleted(tr::now)
					: ErrorText(error));
			}));
		}),
		.confirmText = tr::lng_box_delete(),
	}));
}

rpl::producer<QString> CloudSyncStatus(not_null<Main::Session*> session) {
	const auto user = session->userId().bare;
	auto last = ForAccount(session).Value(
		Sync::kState
	) | rpl::map([=](const QByteArray &raw) {
		const auto state = Sync::ParseState(raw);
		return (state && state->user == user && state->updatedAt)
			? tr::lng_nagram_sync_last(
				tr::now,
				lt_date,
				langDateTime(base::unixtime::parse(state->updatedAt)))
			: tr::lng_nagram_sync_never(tr::now);
	});
	const auto service = Service::Find(session);
	if (!service) {
		return last;
	}
	return rpl::combine(
		std::move(last),
		service->autoIssueValue()
	) | rpl::map([](const QString &last, Error issue) {
		return (issue == Error::None)
			? last
			: (issue == Error::RemoteChanged)
			? tr::lng_nagram_sync_auto_paused(tr::now)
			: tr::lng_nagram_sync_auto_failed_short(tr::now);
	});
}

rpl::producer<bool> CloudSyncAutoValue(not_null<Main::Session*> session) {
	const auto user = session->userId().bare;
	return ForAccount(session).Value(
		Sync::kAutoOwner
	) | rpl::map([=](const QString &owner) {
		return Sync::AutoEnabled(owner, user);
	}) | rpl::distinct_until_changed();
}

void SetCloudSyncAuto(not_null<Main::Session*> session, bool enabled) {
	Expects(ForAccount(session).Set(Sync::kAutoOwner, enabled
		? QString::number(session->userId().bare)
		: QString()));
}

void AttachCloudSync(not_null<Main::Session*> session) {
	crl::on_main(session, [=] {
		const auto issues = session->lifetime().make_state<rpl::lifetime>();
		CloudSyncAutoValue(
			session
		) | rpl::on_next([=](bool enabled) {
			issues->destroy();
			if (!enabled) {
				if (const auto service = Service::Find(session)) {
					service->setAuto(false);
				}
				return;
			}
			auto &service = Service::For(session);
			service.setAuto(true);
			service.autoIssueValue(
			) | rpl::filter([](Error issue) {
				return (issue != Error::None);
			}) | rpl::on_next([=](Error issue) {
				const auto &windows = session->windows();
				if (windows.empty()) {
					return;
				}
				windows.front()->showToast((issue == Error::RemoteChanged)
					? ErrorText(issue)
					: tr::lng_nagram_sync_auto_failed(
						tr::now,
						lt_reason,
						ErrorText(issue)));
			}, *issues);
		}, session->lifetime());
	});
}

} // namespace Nagram
