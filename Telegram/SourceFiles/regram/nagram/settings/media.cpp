#include "regram/nagram/settings/media.h"

#include "regram/nagram/media/options.h"
#include "regram/nagram/media/backend_options.h"
#include "regram/nagram/media/local_faved.h"
#include "regram/nagram/media/local_faved_model.h"
#include "regram/nagram/media/sticker_catalog.h"
#include "regram/nagram/media/sticker_export.h"
#include "regram/nagram/settings/home.h"
#include "regram/nagram/settings/restart.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtCore/QDir>
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

using namespace ::Settings;
using namespace ::Settings::Builder;

class MediaSection final : public Section<MediaSection> {
public:
	MediaSection(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
		build(content, kBuild);
		Ui::ResizeFitChild(this, content);
	}

	[[nodiscard]] rpl::producer<QString> title() override {
		return tr::lng_nagram_media();
	}

	static const SectionBuildMethod kBuild;
};

void AddToggle(
		SectionBuilder &builder,
		const Option<bool> &option,
		rpl::producer<QString> title,
		QString id,
		QStringList keywords) {
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(option),
		.keywords = std::move(keywords),
	});
	if (button) {
		button->toggledChanges(
		) | rpl::on_next([option](bool value) {
			Expects(ForDevice().Set(option, value));
		}, button->lifetime());
	}
}

void StickerScaleBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_sticker_scale());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Media::kStickerScale));
	for (auto value = 50; value <= 200; value += 25) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, QString::number(value) + '%',
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Media::kStickerScale, value));
		box->closeBox();
	});
}

void PanelScaleBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_nagram_sticker_panel_scale());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Media::kStickerPanelScale));
	for (auto value = 50; value <= 200; value += 25) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, QString::number(value) + '%',
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		if (ForDevice().Get(Media::kStickerPanelScale) != value) {
			Expects(ForDevice().Set(Media::kStickerPanelScale, value));
			ShowRestartPrompt(controller);
		}
		box->closeBox();
	});
}

void RecentLimitBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_recent_sticker_limit());
	const auto current = ForDevice().Get(Media::kRecentStickerLimit);
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_recent_sticker_hint(),
		current ? QString::number(current) : QString()));
	field->setInputMethodHints(Qt::ImhDigitsOnly);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		const auto text = field->getLastText().trimmed();
		auto valid = false;
		const auto value = text.isEmpty() ? 0 : text.toInt(&valid);
		if (!text.isEmpty() && (!valid || value < 1 || value > 200)) {
			field->showError();
			return;
		}
		Expects(ForDevice().Set(Media::kRecentStickerLimit, value));
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

QString VoiceBitrateLabel(int value) {
	return value
		? tr::lng_nagram_voice_record_bitrate_value(
			tr::now,
			lt_value,
			QString::number(value))
		: tr::lng_nagram_preview_follow(tr::now);
}

void VoiceBitrateBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_voice_record_bitrate());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Media::kVoiceRecordBitrate));
	for (const auto value : { 0, 16, 24, 48, 64, 96, 128 }) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, VoiceBitrateLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Media::kVoiceRecordBitrate, value));
		box->closeBox();
	});
}

void MusicCoverBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_music_cover_url());
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_nagram_custom_doh_placeholder(),
		ForDevice().Get(Media::kMusicCoverUrl)));
	field->setMaxLength(Media::kMaxCoverUrlLength);
	box->setFocusCallback([=] { field->setFocusFast(); });
	const auto submit = [=] {
		const auto value = field->getLastText().trimmed();
		if (!ForDevice().Set(Media::kMusicCoverUrl, value)) {
			field->showError();
			box->showToast(tr::lng_nagram_external_url_invalid(tr::now));
			return;
		}
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](auto) { submit(); }, field->lifetime());
	box->addButton(tr::lng_settings_save(), submit);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ChooseExportFolder() {
	FileDialog::GetFolder(
		Core::App().getFileDialogParent(),
		tr::lng_nagram_sticker_export_path_choose(tr::now),
		ForDevice().Get(Media::kStickerExportPath),
		[](QString &&result) {
			const auto path = QDir(result).absolutePath();
			if (result.isEmpty()) {
				return;
			} else if (!ForDevice().Set(Media::kStickerExportPath, path)) {
				if (const auto window = Core::App().activeWindow()) {
					window->showToast(
						tr::lng_nagram_sticker_export_error_path(tr::now));
				}
			}
		});
}

void ExportFolderBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_sticker_export_path());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		QDir::toNativeSeparators(ForDevice().Get(Media::kStickerExportPath)),
		st::boxLabel));
	box->addButton(tr::lng_nagram_sticker_export_path_choose(), [=] {
		box->closeBox();
		ChooseExportFolder();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	box->addLeftButton(tr::lng_nagram_sticker_export_path_clear(), [=] {
		Expects(ForDevice().Set(Media::kStickerExportPath, QString()));
		box->closeBox();
	});
}

QString ExportNamingLabel(int value) {
	using Naming = Media::ExportDirNaming;
	switch (static_cast<Naming>(value)) {
	case Naming::ShortName:
		return tr::lng_nagram_sticker_export_dir_short_name(tr::now);
	case Naming::Title:
		return tr::lng_nagram_sticker_export_dir_title(tr::now);
	case Naming::Id: return tr::lng_nagram_sticker_export_dir_id(tr::now);
	}
	Unexpected("Invalid Nagram sticker export naming.");
}

void ExportNamingBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_sticker_export_dir_naming());
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		ForDevice().Get(Media::kStickerExportDirNaming));
	for (auto value = 0; value != Media::kExportDirNamingCount; ++value) {
		box->addRow(object_ptr<Ui::Radiobutton>(
			box, group, value, ExportNamingLabel(value),
			st::settingsSendType), st::settingsSendTypePadding);
	}
	group->setChangedCallback([=](int value) {
		Expects(ForDevice().Set(Media::kStickerExportDirNaming, value));
		box->closeBox();
	});
}

QString ExportStatusLabel(const Media::ExportStatus &status) {
	using Error = Media::ExportError;
	return status.running
		? tr::lng_nagram_sticker_export_progress(
			tr::now,
			lt_index,
			QString::number(status.done),
			lt_amount,
			QString::number(status.total))
		: (status.error == Error::Path)
		? tr::lng_nagram_sticker_export_error_path(tr::now)
		: (status.error == Error::Write)
		? tr::lng_nagram_sticker_export_error_write(tr::now)
		: status.failed
		? tr::lng_nagram_sticker_export_error_download(
			tr::now,
			lt_amount,
			QString::number(status.failed))
		: status.finished
		? tr::lng_nagram_sticker_export_done(
			tr::now,
			lt_amount,
			QString::number(status.done))
		: QString();
}

const auto kMeta = BuildHelper({
	.id = MediaSection::Id(),
	.parentId = HomeId(),
	.title = &tr::lng_nagram_media,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"regram/nagram/media/sticker-scale"_q,
		.title = tr::lng_nagram_sticker_scale(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kStickerScale)
			| rpl::map([](int value) {
				return QString::number(value) + '%';
			}),
		.onClick = [=] { controller->show(Box(StickerScaleBox)); },
		.keywords = { u"sticker"_q, u"size"_q },
	});
	AddToggle(builder, Media::kHideStickerTime,
		tr::lng_nagram_hide_sticker_time(),
		u"regram/nagram/media/hide-sticker-time"_q,
		{ u"sticker"_q, u"time"_q });
	builder.addButton({
		.id = u"regram/nagram/media/recent-sticker-limit"_q,
		.title = tr::lng_nagram_recent_sticker_limit(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kRecentStickerLimit)
			| rpl::map([](int value) {
				return value ? QString::number(value)
					: tr::lng_nagram_preview_follow(tr::now);
			}),
		.onClick = [=] { controller->show(Box(RecentLimitBox)); },
		.keywords = { u"recent"_q, u"sticker"_q, u"limit"_q },
	});
	AddToggle(builder, Media::kHideGroupStickers,
		tr::lng_nagram_hide_group_stickers(),
		u"regram/nagram/media/hide-group-stickers"_q,
		{ u"group"_q, u"sticker"_q });
	AddToggle(builder, Media::kHideRecommendedStickers,
		tr::lng_nagram_hide_recommended_stickers(),
		u"regram/nagram/media/hide-recommended-stickers"_q,
		{ u"recommended"_q, u"sticker"_q });
	AddToggle(builder, Media::kHideRecommendedEmoji,
		tr::lng_nagram_hide_recommended_emoji(),
		u"regram/nagram/media/hide-recommended-emoji"_q,
		{ u"recommended"_q, u"emoji"_q });
	AddToggle(builder, Media::kHideGifCategories,
		tr::lng_nagram_hide_gif_categories(),
		u"regram/nagram/media/hide-gif-categories"_q,
		{ u"GIF"_q, u"categories"_q });
	AddToggle(builder, Media::kHideGreetingSticker,
		tr::lng_nagram_hide_greeting_sticker(),
		u"regram/nagram/media/hide-greeting-sticker"_q,
		{ u"greeting"_q, u"sticker"_q });
	AddToggle(builder, Media::kDisableVideoAutoplay,
		tr::lng_nagram_disable_video_autoplay(),
		u"regram/nagram/media/disable-video-autoplay"_q,
		{ u"video"_q, u"autoplay"_q });
	AddToggle(builder, Media::kGifPlaybackControls,
		tr::lng_nagram_gif_playback_controls(),
		u"regram/nagram/media/gif-playback-controls"_q,
		{ u"GIF"_q, u"playback"_q, u"controls"_q });
	AddToggle(builder, Media::kMp4FilePreview,
		tr::lng_nagram_mp4_file_preview(),
		u"regram/nagram/media/mp4-file-preview"_q,
		{ u"MP4"_q, u"file"_q, u"preview"_q });
	AddToggle(builder, Media::kSmallGifs,
		tr::lng_nagram_small_gifs(),
		u"regram/nagram/media/small-gifs"_q,
		{ u"GIF"_q, u"size"_q, u"small"_q });
	builder.addButton({
		.id = u"regram/nagram/media/sticker-panel-scale"_q,
		.title = tr::lng_nagram_sticker_panel_scale(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kStickerPanelScale)
			| rpl::map([](int value) {
				return QString::number(value) + '%';
			}),
		.onClick = [=] { controller->show(Box(PanelScaleBox, controller)); },
		.keywords = { u"sticker"_q, u"panel"_q, u"size"_q },
	});
	AddToggle(builder, Media::kUnlimitedFavedStickers,
		tr::lng_nagram_unlimited_faved_stickers(),
		u"regram/nagram/media/unlimited-faved-stickers"_q,
		{ u"favorite"_q, u"sticker"_q, u"unlimited"_q, u"limit"_q });
	builder.addDividerText(tr::lng_nagram_unlimited_faved_stickers_about());
	const auto session = builder.session();
	builder.addButton({
		.id = u"regram/nagram/media/local-faved-stickers"_q,
		.title = tr::lng_nagram_local_faved_stickers(),
		.st = &st::settingsButtonNoIcon,
		.label = Media::LocalFavedCountValue(session)
			| rpl::map([](int count) { return QString::number(count); }),
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_local_faved_stickers_clear(),
				.confirmed = [=](Fn<void()> &&close) {
					Media::ClearLocalFaved(session);
					close();
				},
			}));
		},
		.keywords = { u"favorite"_q, u"sticker"_q, u"local"_q },
		.shown = Media::LocalFavedCountValue(session)
			| rpl::map([](int count) { return count > 0; }),
	});
	AddToggle(builder, Media::kDownloadsInMainMenu,
		tr::lng_nagram_downloads_in_main_menu(),
		u"regram/nagram/media/downloads-main-menu"_q,
		{ u"downloads"_q, u"main menu"_q });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/media/auto-download"_q,
		.title = tr::lng_nagram_auto_download(),
		.keywords = { u"auto download"_q },
	});
	AddToggle(builder, Media::kBlockExecutableAutoDownload,
		tr::lng_nagram_block_executable_auto_download(),
		u"regram/nagram/media/block-executables"_q,
		{ u"exe"_q, u"auto download"_q });
	AddToggle(builder, Media::kBlockArchiveAutoDownload,
		tr::lng_nagram_block_archive_auto_download(),
		u"regram/nagram/media/block-archives"_q,
		{ u"zip"_q, u"archive"_q, u"auto download"_q });
	builder.addDividerText(tr::lng_nagram_auto_download_note());
	builder.addButton({
		.id = u"regram/nagram/media/sticker-catalog"_q,
		.title = tr::lng_nagram_catalog_title(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { ShowStickerCatalog(controller); },
		.keywords = { u"sticker"_q, u"catalog"_q },
	});
	const auto exporter = Media::StickerExport::Find(session);
	auto exportPathSet = ForDevice().Value(
		Media::kStickerExportPath
	) | rpl::map([](const QString &path) { return !path.isEmpty(); });
	builder.addSubsectionTitle({
		.id = u"regram/nagram/media/sticker-export"_q,
		.title = tr::lng_nagram_sticker_export(),
		.keywords = { u"sticker"_q, u"export"_q, u"folder"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/media/sticker-export-path"_q,
		.title = tr::lng_nagram_sticker_export_path(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kStickerExportPath)
			| rpl::map([](const QString &path) {
				return path.isEmpty()
					? tr::lng_nagram_sticker_export_path_unset(tr::now)
					: QDir(path).dirName();
			}),
		.onClick = [=] {
			if (ForDevice().Get(Media::kStickerExportPath).isEmpty()) {
				ChooseExportFolder();
			} else {
				controller->show(Box(ExportFolderBox));
			}
		},
		.keywords = { u"sticker"_q, u"export"_q, u"folder"_q },
	});
	builder.addDividerText(tr::lng_nagram_sticker_export_path_about());
	const auto autoSync = builder.addButton({
		.id = u"regram/nagram/media/sticker-export-auto-sync"_q,
		.title = tr::lng_nagram_sticker_export_auto_sync(),
		.st = &st::settingsButtonNoIcon,
		.toggled = ForDevice().Value(Media::kStickerExportAutoSync),
		.keywords = { u"sticker"_q, u"export"_q, u"sync"_q },
		.shown = rpl::duplicate(exportPathSet),
	});
	if (autoSync) {
		autoSync->toggledChanges(
		) | rpl::on_next([](bool value) {
			Expects(ForDevice().Set(Media::kStickerExportAutoSync, value));
		}, autoSync->lifetime());
	}
	builder.addButton({
		.id = u"regram/nagram/media/sticker-export-naming"_q,
		.title = tr::lng_nagram_sticker_export_dir_naming(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kStickerExportDirNaming)
			| rpl::map(ExportNamingLabel),
		.onClick = [=] { controller->show(Box(ExportNamingBox)); },
		.keywords = { u"sticker"_q, u"export"_q, u"folder name"_q },
		.shown = rpl::duplicate(exportPathSet),
	});
	builder.addButton({
		.id = u"regram/nagram/media/sticker-export-sync"_q,
		.title = tr::lng_nagram_sticker_export_sync_now(),
		.st = &st::settingsButtonNoIcon,
		.label = (exporter
			? exporter->statusValue()
			: rpl::single(Media::ExportStatus())
		) | rpl::map(ExportStatusLabel),
		.onClick = [=] {
			if (const auto current = Media::StickerExport::Find(session)) {
				current->syncNow();
			}
		},
		.keywords = { u"sticker"_q, u"export"_q, u"sync"_q },
		.shown = std::move(exportPathSet),
	});
	builder.addDividerText(tr::lng_nagram_sticker_export_sync_now_about());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/media/recording-calls"_q,
		.title = tr::lng_nagram_recording_calls(),
		.keywords = { u"recording"_q, u"calls"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/media/voice-bitrate"_q,
		.title = tr::lng_nagram_voice_record_bitrate(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kVoiceRecordBitrate)
			| rpl::map(VoiceBitrateLabel),
		.onClick = [=] { controller->show(Box(VoiceBitrateBox)); },
		.keywords = { u"voice"_q, u"bitrate"_q, u"recording"_q },
	});
	builder.addDividerText(tr::lng_nagram_voice_record_bitrate_about());
	AddToggle(builder, Media::kGroupCallRawAudio,
		tr::lng_nagram_group_call_raw_audio(),
		u"regram/nagram/media/group-call-raw-audio"_q,
		{ u"group call"_q, u"audio"_q, u"noise"_q, u"echo"_q });
	builder.addDividerText(tr::lng_nagram_group_call_raw_audio_about());
	builder.addSubsectionTitle({
		.id = u"regram/nagram/media/music-cover"_q,
		.title = tr::lng_nagram_music_cover(),
		.keywords = { u"music"_q, u"cover"_q },
	});
	builder.addButton({
		.id = u"regram/nagram/media/cover-url"_q,
		.title = tr::lng_nagram_music_cover_url(),
		.st = &st::settingsButtonNoIcon,
		.label = ForDevice().Value(Media::kMusicCoverUrl)
			| rpl::map([](const QString &value) {
				return value.isEmpty()
					? tr::lng_nagram_external_url_default(tr::now)
					: Media::CoverUrlHost(value);
			}),
		.onClick = [=] { controller->show(Box(MusicCoverBox)); },
		.keywords = { u"music"_q, u"cover"_q, u"artwork"_q, u"API"_q },
	});
	builder.addDividerText(tr::lng_nagram_music_cover_url_about(
		lt_artist,
		rpl::single(u"{artist}"_q),
		lt_title,
		rpl::single(u"{title}"_q)));
});

const SectionBuildMethod MediaSection::kBuild = kMeta.build;

} // namespace

Settings::Type MediaId() {
	return MediaSection::Id();
}

} // namespace Nagram
