#pragma once

#include "base/basic_types.h"

#include <optional>

namespace Nagram::Chats::Cleanup {

enum class Category {
	DeletedAccount,
	UnusedBot,
	InactiveChat,
};

struct Facts {
	bool user = false;
	bool deleted = false;
	bool bot = false;
	bool kept = false;
	TimeId lastActivity = 0;
};

inline constexpr auto kInactiveDays = 180;

[[nodiscard]] std::optional<Category> Classify(
	const Facts &facts,
	TimeId now);

} // namespace Nagram::Chats::Cleanup
