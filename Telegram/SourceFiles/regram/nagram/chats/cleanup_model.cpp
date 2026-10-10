#include "regram/nagram/chats/cleanup_model.h"

namespace Nagram::Chats::Cleanup {
namespace {

constexpr auto kInactiveSeconds = int64(kInactiveDays) * 86400;

} // namespace

std::optional<Category> Classify(const Facts &facts, TimeId now) {
	if (facts.kept) {
		return std::nullopt;
	} else if (facts.user && facts.deleted) {
		return Category::DeletedAccount;
	}
	const auto inactive = (facts.lastActivity > 0)
		&& (int64(now) - facts.lastActivity >= kInactiveSeconds);
	if (!inactive || (facts.user && !facts.bot)) {
		return std::nullopt;
	}
	return facts.user ? Category::UnusedBot : Category::InactiveChat;
}

} // namespace Nagram::Chats::Cleanup
