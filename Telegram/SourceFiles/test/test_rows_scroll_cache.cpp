#ifdef _DEBUG

#include "test/test_rows_scroll_cache.h"

#include "test/test_log.h"
#include "ui/rows_scroll_cache.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>

namespace Test {

void VerifyRowsScrollCache() {
	auto canvas = QImage(QSize(256, 256), QImage::Format_RGB32);
	auto painter = QPainter(&canvas);
	auto evicted = std::vector<uint64>();
	auto cache = Ui::RowsScrollCache([] {}, [&](uint64 id) {
		evicted.push_back(id);
	});
	auto painted = 0;
	const auto paint = [&](uint64 id, QSize size, int ratio = 1) {
		cache.paintRow(painter, id, size, ratio, [&](QImage &image) {
			++painted;
			image.fill(QColor::fromRgb(uint(id)));
		});
	};
	const auto size = canvas.size();
	auto largestMissBurst = 0;
	for (auto top = 0; top != 500; ++top) {
		const auto before = painted;
		for (auto row = top; row != top + 16; ++row) {
			paint(row, size);
		}
		if (top) {
			largestMissBurst = std::max(largestMissBurst, painted - before);
		}
	}
	Check(!evicted.empty() && largestMissBurst == 1,
		u"Long scrolling does not discard the visible cached rows"_q);
	auto retainedBytes = int64(0);
	for (auto row = 0; row != 516; ++row) {
		if (cache.hasFresh(row, size, 1)) {
			retainedBytes += canvas.sizeInBytes();
		}
	}
	Check(retainedBytes <= 32 * 1024 * 1024,
		u"Scrolling cache remains within its memory budget"_q);

	cache.clear();
	evicted.clear();
	const auto tiny = QSize(1, 1);
	for (auto id = uint64(1); id <= 256; ++id) {
		paint(id, tiny);
	}
	paint(1, tiny);
	paint(257, tiny);
	Check(cache.hasFresh(1, tiny, 1)
		&& !cache.hasFresh(2, tiny, 1)
		&& cache.hasFresh(257, tiny, 1)
		&& evicted == std::vector<uint64>{ 2 },
		u"Capacity eviction preserves the most recently painted row"_q);
	cache.invalidate(1);
	Check(!cache.hasFresh(1, tiny, 1) && cache.hasFresh(257, tiny, 1),
		u"Invalidating one row preserves unrelated cached content"_q);

	cache.clear();
	paint(1, size);
	paint(2, size);
	const auto beforeRatioChange = painted;
	Check(!cache.hasFresh(1, size, 2),
		u"A cached image from another display scale is rejected"_q);
	paint(1, size, 2);
	Check(painted == beforeRatioChange + 1 && cache.hasFresh(1, size, 2),
		u"Display scale changes regenerate the row"_q);
	const auto beforeReuse = painted;
	paint(1, size, 2);
	Check(painted == beforeReuse,
		u"Unchanged rows reuse their prepared image"_q);
	paint(1, QSize(128, 128), 2);
	Check(cache.hasFresh(1, QSize(128, 128), 2),
		u"Window geometry changes replace cached row dimensions"_q);

	paint(3, QSize(4096, 2049));
	Check(!cache.hasFresh(3, QSize(4096, 2049), 1)
		&& cache.hasFresh(2, size, 1),
		u"An oversized row does not evict existing reusable images"_q);
	cache.clear();
	Check(!cache.hasFresh(1, QSize(128, 128), 2),
		u"Explicit refresh drops old row content"_q);
}

} // namespace Test

#endif // _DEBUG
