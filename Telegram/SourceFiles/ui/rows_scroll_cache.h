/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "base/flat_map.h"

namespace Ui {

class RowsScrollCache final {
public:
	explicit RowsScrollCache(
		Fn<void()> stopped,
		Fn<void(uint64)> evicted = nullptr);

	void markScrolling();
	[[nodiscard]] bool scrolling() const {
		return _scrolling;
	}
	[[nodiscard]] bool hasFresh(
			uint64 rowId,
			QSize physicalSize,
			int ratio) const {
		const auto i = _images.find(rowId);
		return (i != end(_images))
			&& (i->second.image.size() == physicalSize)
			&& (i->second.image.devicePixelRatio() == ratio);
	}

	template <typename PaintToImage>
	void paintRow(
			QPainter &p,
			uint64 rowId,
			QSize physicalSize,
			int ratio,
			PaintToImage &&paintToImage) {
		if (const auto image = find(rowId, physicalSize, ratio)) {
			p.drawImage(0, 0, *image);
			return;
		}
		auto image = QImage(physicalSize, QImage::Format_RGB32);
		image.setDevicePixelRatio(ratio);
		paintToImage(image);
		p.drawImage(0, 0, image);
		cache(rowId, std::move(image));
	}

	void invalidate(uint64 rowId);
	void clear();

private:
	struct Cached {
		QImage image;
		uint64 used = 0;
	};

	[[nodiscard]] const QImage *find(uint64 rowId, QSize size, int ratio);
	void cache(uint64 rowId, QImage image);

	static constexpr auto kLimit = 256;
	static constexpr auto kMemoryLimit = 32 * 1024 * 1024;

	base::flat_map<uint64, Cached> _images;
	base::Timer _stopTimer;
	Fn<void(uint64)> _evicted;
	int64 _memory = 0;
	uint64 _usage = 0;
	bool _scrolling = false;

};

} // namespace Ui
