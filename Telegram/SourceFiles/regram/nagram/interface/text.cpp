#include "regram/nagram/interface/text.h"

#include "regram/nagram/interface/options.h"

namespace Nagram::Interface {
namespace {

bool &HalfwidthEnabled() {
	static auto enabled = false;
	return enabled;
}

} // namespace

QString HalfwidthPunctuation(QString value) {
	for (auto &ch : value) {
		const auto code = ch.unicode();
		if (code >= 0xFF01 && code <= 0xFF5E) {
			ch = QChar(code - 0xFEE0);
		} else switch (code) {
		case 0x3000: ch = u' '; break;
		case 0x3001: ch = u','; break;
		case 0x3002: ch = u'.'; break;
		case 0x300A: case 0x300B: ch = code == 0x300A ? u'<' : u'>'; break;
		case 0x3010: case 0x3011: ch = code == 0x3010 ? u'[' : u']'; break;
		case 0x2018: case 0x2019: ch = u'\''; break;
		case 0x201C: case 0x201D: ch = u'"'; break;
		default: break;
		}
	}
	return value;
}

QString UiText(QString value) {
	return HalfwidthEnabled()
		? HalfwidthPunctuation(std::move(value))
		: value;
}

void StartUiText() {
	HalfwidthEnabled() = ForDevice().Get(kHalfwidthUiPunctuation);
}

} // namespace Nagram::Interface
