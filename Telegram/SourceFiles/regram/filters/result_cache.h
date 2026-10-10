#pragma once

#include "base/weak_ptr.h"

#include <QtCore/QCache>
#include <QtCore/QString>

namespace Regram::Filters {

struct MessageKey {
	uint64 account = 0;
	uint64 peer = 0;
	int64 message = 0;
	friend bool operator==(const MessageKey &, const MessageKey &) = default;
};

[[nodiscard]] size_t qHash(const MessageKey &key, size_t seed = 0);

class ResultCache final {
public:
	explicit ResultCache(int maximumCost = 8 * 1024 * 1024);

	[[nodiscard]] std::optional<bool> get(
		MessageKey key,
		not_null<const base::has_weak_ptr*> owner,
		const QString &text);
	void put(
		MessageKey key,
		not_null<const base::has_weak_ptr*> owner,
		QString text,
		bool hidden);
	[[nodiscard]] bool hasHidden(
		uint64 account,
		uint64 peer,
		not_null<const base::has_weak_ptr*> owner);
	void remove(MessageKey key);
	void clear();
	[[nodiscard]] int totalCost() const;

private:
	struct Verdict {
		base::weak_ptr<const base::has_weak_ptr> owner;
		QString text;
		bool hidden = false;
	};
	QCache<MessageKey, Verdict> _entries;

};

} // namespace Regram::Filters
