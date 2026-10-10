#pragma once

#include <gsl/pointers>

class PeerData;
class HistoryItem;

namespace Data {
struct PossibleItemReactionsRef;
} // namespace Data
namespace HistoryView {
class Element;
namespace Reactions {
struct ButtonParameters;
struct InlineListData;
} // namespace Reactions
} // namespace HistoryView

namespace Nagram::Messages {

[[nodiscard]] bool ReactionsHidden(not_null<const PeerData*> peer);
[[nodiscard]] HistoryView::Reactions::InlineListData FilterInlineReactions(
	not_null<HistoryView::Element*> view,
	HistoryView::Reactions::InlineListData data);
[[nodiscard]] HistoryView::Reactions::ButtonParameters FilterReactionButton(
	not_null<const PeerData*> peer,
	HistoryView::Reactions::ButtonParameters parameters);
[[nodiscard]] Data::PossibleItemReactionsRef MenuReactions(
	not_null<HistoryItem*> item);
[[nodiscard]] bool AllowReactionSelector(bool hasSelection);
[[nodiscard]] bool WhoReactedAtTop();

} // namespace Nagram::Messages
