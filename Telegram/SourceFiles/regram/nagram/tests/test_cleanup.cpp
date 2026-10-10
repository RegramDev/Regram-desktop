#include "regram/nagram/chats/cleanup_model.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestCleanup() {
	using namespace Nagram::Chats::Cleanup;
	constexpr auto kDay = 86400;
	constexpr auto kNow = TimeId(1'800'000'000);
	constexpr auto kOld = kNow - kInactiveDays * kDay;

	Require(Classify({ .user = true, .deleted = true }, kNow)
			== Category::DeletedAccount
		&& Classify({ .user = true, .deleted = true, .lastActivity = kNow },
			kNow) == Category::DeletedAccount,
		"cleanup offers deleted accounts regardless of activity");
	Require(Classify({ .user = true, .bot = true, .lastActivity = kOld },
			kNow) == Category::UnusedBot
		&& !Classify({ .user = true, .bot = true, .lastActivity = kOld + 1 },
			kNow),
		"cleanup offers bots only after the inactive period");
	Require(Classify({ .lastActivity = kOld }, kNow) == Category::InactiveChat
		&& !Classify({ .lastActivity = kOld + 1 }, kNow),
		"cleanup offers groups only after the inactive period");
	Require(!Classify({ .user = true, .lastActivity = kOld - kDay }, kNow)
		&& !Classify({ .deleted = true, .lastActivity = kNow }, kNow),
		"cleanup never offers people or active groups");
	Require(!Classify({ .user = true, .bot = true }, kNow)
		&& !Classify({}, kNow)
		&& !Classify({ .lastActivity = -5 }, kNow)
		&& !Classify({ .lastActivity = kNow + kDay }, kNow),
		"cleanup skips chats with unknown or future activity");
	Require(!Classify({ .kept = true, .lastActivity = kOld }, kNow)
		&& !Classify({ .user = true, .deleted = true, .kept = true }, kNow),
		"cleanup never offers kept chats");
	Require(Classify({ .lastActivity = 1 }, std::numeric_limits<TimeId>::max())
			== Category::InactiveChat,
		"cleanup period does not overflow");
	std::cout << "PASS: Nagram chat cleanup" << std::endl;
}
