#pragma once

#include <gsl/pointers>
#include <rpl/rpl.h>

#include <memory>
#include <vector>

class HistoryItem;
class PeerData;

namespace Data {
struct ReactionId;
} // namespace Data
namespace Ui {
class RpWidget;
} // namespace Ui
namespace HistoryView {
class Element;
class Media;
} // namespace HistoryView

namespace Nagram::Messages {

[[nodiscard]] bool RevealTextSpoilers();
[[nodiscard]] bool RevealMediaSpoiler(not_null<HistoryItem*> item);
[[nodiscard]] bool HideQuickShare();
[[nodiscard]] bool HideRecommendedChannels();
[[nodiscard]] std::unique_ptr<HistoryView::Media> RecommendedChannelsMedia(
	not_null<HistoryView::Element*> view);
[[nodiscard]] bool CollapseRecommendedChannels();
[[nodiscard]] bool SimplifiedReplies();
[[nodiscard]] bool VideoUserpicAllowed(not_null<PeerData*> peer);
[[nodiscard]] bool HideSavedTags();
[[nodiscard]] rpl::producer<bool> SavedTagsValue();
[[nodiscard]] bool ShowSavedTag(
	Data::ReactionId id,
	const std::vector<Data::ReactionId> &selected,
	const std::vector<Data::ReactionId> &added);
[[nodiscard]] bool HidePrivateActivity(const PeerData *peer);
void AttachActivityRefresh(not_null<Ui::RpWidget*> widget);

} // namespace Nagram::Messages
