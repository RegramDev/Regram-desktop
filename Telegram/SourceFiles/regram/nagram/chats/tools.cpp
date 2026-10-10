#include "regram/nagram/chats/tools.h"

#include "regram/nagram/chats/options.h"
#include "apiwrap.h"
#include "data/data_peer.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/notify/data_notify_settings.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "history/view/history_view_pinned_section.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "storage/storage_shared_media.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "window/window_session_controller.h"
#include "styles/style_nagram_compose.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QResizeEvent>

namespace Nagram::Chats {
namespace {

constexpr auto kObjectName = "nagramChatTools";

struct ToolIcon {
	const style::icon *normal = nullptr;
	const style::icon *over = nullptr;
};

class ChatTools final : public Ui::RpWidget {
public:
	ChatTools(
		not_null<Ui::RpWidget*> bar,
		not_null<Window::SessionController*> controller);

	void setHistory(History *history);
	[[nodiscard]] History *history() const {
		return _history;
	}

private:
	void rebuild();
	void layoutButtons();
	void relayoutBar();
	Ui::IconButton *addButton(
		ToolIcon icon,
		const QString &name,
		Fn<void()> callback);

	const not_null<Ui::RpWidget*> _bar;
	const not_null<Window::SessionController*> _controller;
	History *_history = nullptr;
	std::vector<base::unique_qptr<Ui::IconButton>> _buttons;
	rpl::lifetime _stateLifetime;

};

ChatTools::ChatTools(
	not_null<Ui::RpWidget*> bar,
	not_null<Window::SessionController*> controller)
: RpWidget(bar)
, _bar(bar)
, _controller(controller) {
	setObjectName(QString::fromLatin1(kObjectName));
	hide();
	ForDevice().Value(kChatTools) | rpl::skip(1) | rpl::on_next([=] {
		relayoutBar();
	}, lifetime());
}

Ui::IconButton *ChatTools::addButton(
		ToolIcon icon,
		const QString &name,
		Fn<void()> callback) {
	auto button = base::make_unique_q<Ui::IconButton>(
		this,
		st::nagramChatToolsButton);
	button->setIconOverride(icon.normal, icon.over);
	button->setAccessibleName(name);
	button->setClickedCallback(std::move(callback));
	button->show();
	const auto raw = button.get();
	_buttons.push_back(std::move(button));
	return raw;
}

void ChatTools::layoutButtons() {
	auto left = 0;
	auto height = 0;
	for (const auto &button : _buttons) {
		if (!button->isHidden()) {
			button->moveToLeft(left, 0);
			left += button->width();
			height = button->height();
		}
	}
	resize(left, height);
}

void ChatTools::relayoutBar() {
	auto event = QResizeEvent(_bar->size(), _bar->size());
	QCoreApplication::sendEvent(_bar, &event);
}

void ChatTools::setHistory(History *history) {
	if (_history != history) {
		_history = history;
		rebuild();
	}
}

void ChatTools::rebuild() {
	_stateLifetime.destroy();
	_buttons.clear();
	resize(0, 0);
	if (!_history) {
		return;
	}
	const auto history = _history;
	const auto peer = history->peer;
	const auto controller = _controller;
	if (peer->sharedMediaInfo()) {
		addButton({
			&st::nagramChatToolsPhoto,
			&st::nagramChatToolsPhotoOver,
		}, tr::lng_media_type_photos(tr::now), [=] {
			controller->showSection(std::make_shared<Info::Memento>(
				peer,
				Info::Section(Storage::SharedMediaType::Photo)));
		});
	}
	const auto pinned = addButton(
		{ &st::nagramChatToolsPin, &st::nagramChatToolsPinOver },
		tr::lng_pinned_message(tr::now),
		[=] {
			controller->showSection(
				std::make_shared<HistoryView::PinnedMemento>(history));
		});
	pinned->setVisible(history->hasPinnedMessages());
	const auto weak = base::make_weak(controller);
	const auto start = tr::lng_nagram_chat_tools_start(tr::now);
	addButton({
		&st::nagramChatToolsStart,
		&st::nagramChatToolsStartOver,
	}, start, [=] {
		peer->session().api().resolveJumpToDate(
			Dialogs::Key(history),
			QDate(2013, 8, 1),
			[=](not_null<PeerData*> target, MsgId id) {
				if (const auto strong = weak.get()) {
					strong->showPeerHistory(
						target,
						Window::SectionShow::Way::Forward,
						id);
				}
			});
	});
	const auto mute = addButton({
		&st::nagramChatToolsMute,
		&st::nagramChatToolsMuteOver,
	}, QString(), [=] {
		auto &settings = peer->owner().notifySettings();
		settings.update(history, settings.isMuted(history)
			? Data::MuteValue{ .unmute = true }
			: Data::MuteValue{ .forever = true });
	});
	Info::Profile::NotificationsEnabledValue(
		history
	) | rpl::on_next([=](bool enabled) {
		mute->setIconOverride(
			enabled ? &st::nagramChatToolsMute : &st::nagramChatToolsUnmute,
			(enabled
				? &st::nagramChatToolsMuteOver
				: &st::nagramChatToolsUnmuteOver));
		mute->setAccessibleName((enabled
			? tr::lng_context_mute
			: tr::lng_context_unmute)(tr::now));
	}, _stateLifetime);
	peer->session().changes().entryUpdates(
		history,
		Data::EntryUpdate::Flag::HasPinnedMessages
	) | rpl::on_next([=] {
		pinned->setVisible(history->hasPinnedMessages());
		layoutButtons();
		relayoutBar();
	}, _stateLifetime);
	layoutButtons();
}

} // namespace

int LayoutChatTools(
		not_null<Ui::RpWidget*> bar,
		not_null<Window::SessionController*> controller,
		const Dialogs::Key &key,
		bool allowed,
		int right,
		int top) {
	auto tools = static_cast<ChatTools*>(bar->findChild<Ui::RpWidget*>(
		QString::fromLatin1(kObjectName),
		Qt::FindDirectChildrenOnly));
	if (!tools) {
		tools = new ChatTools(bar, controller);
	}
	const auto history = (allowed && ForDevice().Get(kChatTools))
		? key.history()
		: nullptr;
	tools->setHistory(history);
	if (!history || !tools->width()) {
		tools->hide();
		return 0;
	}
	tools->moveToRight(right, top);
	tools->show();
	tools->raise();
	return tools->width();
}

} // namespace Nagram::Chats
