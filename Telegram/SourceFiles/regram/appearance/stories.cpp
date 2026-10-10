#include "regram/appearance/stories.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "dialogs/ui/dialogs_stories_list.h"
#include "regram/nagram/chats/options.h"

namespace Regram::Appearance {
namespace {

constexpr auto kHideStories = "regram.appearance.hide-stories";

} // namespace

bool HideStories() {
	return Nagram::ForDevice().Get(Nagram::Chats::kHideStories);
}

rpl::producer<bool> HideStoriesValue() {
	return Nagram::ForDevice().Value(Nagram::Chats::kHideStories);
}

void SetHideStories(bool hide) {
	Core::App().settings().writePref<bool>(kHideStories, hide);
	Expects(Nagram::ForDevice().Set(Nagram::Chats::kHideStories, hide));
}

rpl::producer<Dialogs::Stories::Content> StoriesContent(
		not_null<Main::Session*> session,
		Data::StorySourcesList list) {
	return rpl::combine(
		Dialogs::Stories::ContentForSession(session, list),
		HideStoriesValue()
	) | rpl::map([](Dialogs::Stories::Content content, bool hide) {
		return hide ? Dialogs::Stories::Content() : std::move(content);
	});
}

} // namespace Regram::Appearance
