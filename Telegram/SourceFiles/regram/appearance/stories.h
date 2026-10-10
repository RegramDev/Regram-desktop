#pragma once

#include "dialogs/ui/dialogs_stories_content.h"

namespace Regram::Appearance {

[[nodiscard]] bool HideStories();
[[nodiscard]] rpl::producer<bool> HideStoriesValue();
void SetHideStories(bool hide);

[[nodiscard]] rpl::producer<Dialogs::Stories::Content> StoriesContent(
	not_null<Main::Session*> session,
	Data::StorySourcesList list);

} // namespace Regram::Appearance
