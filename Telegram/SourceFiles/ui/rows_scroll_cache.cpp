/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/rows_scroll_cache.h"

namespace Ui {
namespace {

constexpr auto kStopTimeout = crl::time(120);

} // namespace

RowsScrollCache::RowsScrollCache(
		Fn<void()> stopped,
		Fn<void(uint64)> evicted)
: _evicted(std::move(evicted)) {
	_stopTimer.setCallback([=] {
		_scrolling = false;
		clear();
		stopped();
	});
}

void RowsScrollCache::markScrolling() {
	_scrolling = true;
	_stopTimer.callOnce(kStopTimeout);
}

void RowsScrollCache::invalidate(uint64 rowId) {
	const auto i = _images.find(rowId);
	if (i != end(_images)) {
		_memory -= i->second.image.sizeInBytes();
		_images.erase(i);
	}
}

const QImage *RowsScrollCache::find(uint64 rowId, QSize size, int ratio) {
	const auto i = _images.find(rowId);
	if (i == end(_images)
		|| i->second.image.size() != size
		|| i->second.image.devicePixelRatio() != ratio) {
		return nullptr;
	}
	i->second.used = ++_usage;
	return &i->second.image;
}

void RowsScrollCache::cache(uint64 rowId, QImage image) {
	invalidate(rowId);
	const auto cost = image.sizeInBytes();
	if (cost > kMemoryLimit || image.isNull()) {
		return;
	}
	while (!_images.empty()
		&& (_images.size() >= kLimit || _memory + cost > kMemoryLimit)) {
		const auto oldest = ranges::min_element(
			_images,
			std::less<>(),
			[](const auto &entry) { return entry.second.used; });
		const auto id = oldest->first;
		invalidate(id);
		if (_evicted) {
			_evicted(id);
		}
	}
	_memory += cost;
	_images.emplace(rowId, Cached{ std::move(image), ++_usage });
}

void RowsScrollCache::clear() {
	_images.clear();
	_memory = 0;
	_usage = 0;
}

} // namespace Ui
