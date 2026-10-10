// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "regram/ayu/features/filters/filters_cache_controller.h"

#include "regram/ayu/data/ayu_database.h"
#include "regram/ayu/features/filters/filters_controller.h"
#include "data/data_groups.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "base/weak_ptr.h"
#include "regram/filters/result_cache.h"
#include "main/main_session.h"
#include "rpl/event_stream.h"

#include <QtCore/QCache>

#include <memory>
#include <mutex>
#include <unordered_set>

namespace FiltersCacheController {

namespace {

using MessageKey = Regram::Filters::MessageKey;

struct DialogKey {
	uint64 account = 0;
	uint64 peer = 0;
	friend bool operator==(const DialogKey &, const DialogKey &) = default;
};

size_t qHash(const DialogKey &key, size_t seed = 0) {
	return qHashMulti(seed, key.account, key.peer);
}

MessageKey Key(not_null<HistoryItem*> item) {
	return { item->history()->session().uniqueId(),
		item->history()->peer->id.value, item->id.bare };
}

DialogKey Key(not_null<PeerData*> peer) {
	return { peer->session().uniqueId(), peer->id.value };
}

} // namespace

std::mutex rebuildMutex;
std::mutex cacheMutex;
std::mutex filteredMessagesMutex;

rpl::event_stream<> filtersUpdateStream;

void fireUpdate() {
	filtersUpdateStream.fire({});
}

rpl::producer<> updates() {
	return filtersUpdateStream.events();
}

std::shared_ptr<const Cache> cache;

Regram::Filters::ResultCache filteredMessages;
QCache<DialogKey, base::weak_ptr<Main::Session>> dialogsWithHiddenBlockedMessages(1000);

std::shared_ptr<const Cache> buildCache() {
	const auto filters = AyuDatabase::getAllRegexFilters();
	const auto exclusions = AyuDatabase::getAllFiltersExclusions();

	std::vector<HashablePattern> shared;
	std::unordered_map<ID, std::vector<ReversiblePattern>> byDialogId;

	for (const auto &filter : filters) {
		if (!filter.enabled || filter.text.empty()) {
			continue;
		}

		int flags = UREGEX_MULTILINE;
		if (filter.caseInsensitive) flags |= UREGEX_CASE_INSENSITIVE;

		auto status = U_ZERO_ERROR;
		auto pattern = icu::RegexPattern::compile(icu::UnicodeString::fromUTF8(filter.text), flags, status);

		if (!pattern) {
			continue;
		}

		if (filter.dialogId.has_value()) {
			byDialogId[filter.dialogId.value()].push_back({
				std::shared_ptr<icu::RegexPattern>(pattern),
				filter.reversed,
			});
		} else {
			shared.push_back({
				filter.id,
				{
					std::shared_ptr<icu::RegexPattern>(pattern),
					filter.reversed,
				},
			});
		}
	}

	auto exclByDialogId = buildExclusions(exclusions, shared);
	auto result = std::make_shared<Cache>();
	result->sharedPatterns = std::move(shared);
	result->patternsByDialogId = std::move(byDialogId);
	result->exclusionsByDialogId = std::move(exclByDialogId);

	return result;
}

void rebuildCache() {
	std::lock_guard rebuildLock(rebuildMutex);
	auto next = buildCache();
	{
		std::lock_guard cacheLock(cacheMutex);
		std::lock_guard filteredLock(filteredMessagesMutex);
		cache = std::move(next);
		filteredMessages.clear();
		dialogsWithHiddenBlockedMessages.clear();
	}
}

std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> buildExclusions(
	const std::vector<RegexFilterGlobalExclusion> &exclusions,
	const std::vector<HashablePattern> &shared) {
	std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> exclusionsByDialogId;

	for (const auto &exclusion : exclusions) {
		auto &exclusionSet = exclusionsByDialogId[exclusion.dialogId];

		for (const auto &filter : shared) {
			if (filter.id == exclusion.filterId) {
				exclusionSet.insert(filter);
				break;
			}
		}
	}
	return exclusionsByDialogId;
}

std::shared_ptr<const Cache> snapshot() {
	{
		std::lock_guard lock(cacheMutex);
		if (cache) {
			return cache;
		}
	}

	std::lock_guard rebuildLock(rebuildMutex);
	{
		std::lock_guard lock(cacheMutex);
		if (cache) {
			return cache;
		}
	}

	auto next = buildCache();
	std::lock_guard lock(cacheMutex);
	if (!cache) {
		cache = std::move(next);
	}
	return cache;
}

std::optional<bool> isFiltered(not_null<HistoryItem*> item) {
	std::lock_guard lock(filteredMessagesMutex);
	return filteredMessages.get(Key(item), &item->history()->session(), item->originalText().text);
}

bool hasFilteredMessages(not_null<PeerData*> peer) {
	std::lock_guard lock(filteredMessagesMutex);
	const auto key = Key(peer);
	const auto blocked = dialogsWithHiddenBlockedMessages.object(key);
	if (blocked && blocked->get() == &peer->session()) {
		return true;
	}
	return filteredMessages.hasHidden(key.account, key.peer, &peer->session());
}

void putHiddenBlockedMessage(not_null<HistoryItem*> item) {
	std::lock_guard lock(filteredMessagesMutex);
	dialogsWithHiddenBlockedMessages.insert(Key(item->history()->peer),
		new base::weak_ptr<Main::Session>(base::make_weak(&item->history()->session())));
}

void putFiltered(
		not_null<HistoryItem*> item,
		const Data::Group *group,
		bool res,
		const std::shared_ptr<const Cache> &matchedCache) {
	std::lock_guard cacheLock(cacheMutex);
	if (cache != matchedCache) {
		return;
	}

	std::lock_guard filteredLock(filteredMessagesMutex);
	const auto put = [](not_null<HistoryItem*> item, bool res) {
		filteredMessages.put(Key(item), &item->history()->session(), item->originalText().text, res);
	};
	put(item, res);
	if (group && res) {
		for (const auto& groupItem : group->items) {
			put(groupItem, true);
		}
	}
}

void invalidateSingle(not_null<HistoryItem*> item) {
	filteredMessages.remove(Key(item));
}

void invalidate(not_null<HistoryItem*> item) {
	std::lock_guard lock(filteredMessagesMutex);
	if (const auto group = item->history()->owner().groups().find(item)) {
		for (const auto& groupItem : group->items) {
			invalidateSingle(groupItem);
		}
	} else {
		invalidateSingle(item);
	}
}

}
