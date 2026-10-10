#include "regram/filters/result_cache.h"

namespace Regram::Filters {

size_t qHash(const MessageKey &key, size_t seed) {
	return qHashMulti(seed, key.account, key.peer, key.message);
}

ResultCache::ResultCache(int maximumCost) : _entries(maximumCost) {
}

std::optional<bool> ResultCache::get(
		MessageKey key,
		not_null<const base::has_weak_ptr*> owner,
		const QString &text) {
	const auto verdict = _entries.object(key);
	return (!verdict || verdict->owner.get() != owner.get() || verdict->text != text)
		? std::nullopt : std::optional(verdict->hidden);
}

void ResultCache::put(
		MessageKey key,
		not_null<const base::has_weak_ptr*> owner,
		QString text,
		bool hidden) {
	const auto cost = std::max(1024, int(text.size() * sizeof(QChar) + sizeof(Verdict)));
	_entries.insert(key, new Verdict{ base::make_weak(owner), std::move(text), hidden }, cost);
}

bool ResultCache::hasHidden(
		uint64 account,
		uint64 peer,
		not_null<const base::has_weak_ptr*> owner) {
	for (const auto key : _entries.keys()) {
		if (key.account == account && key.peer == peer) {
			const auto verdict = _entries.object(key);
			if (verdict->hidden && verdict->owner.get() == owner.get()) {
				return true;
			}
		}
	}
	return false;
}

void ResultCache::remove(MessageKey key) {
	_entries.remove(key);
}

void ResultCache::clear() {
	_entries.clear();
}

int ResultCache::totalCost() const {
	return _entries.totalCost();
}

} // namespace Regram::Filters
