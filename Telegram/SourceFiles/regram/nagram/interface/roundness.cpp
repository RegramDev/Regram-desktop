#include "regram/nagram/interface/roundness.h"

#include "regram/nagram/interface/options.h"
#include "regram/ayu/ui/ayu_userpic.h"
#include "data/data_peer.h"
#include "media/streaming/media_streaming_instance.h"
#include "ui/image/image_prepare.h"
#include "ui/style/style_core.h"

#include <algorithm>

namespace Nagram::Interface {
namespace {

int BubbleRoundness = 0;

} // namespace

void StartRoundness() {
	BubbleRoundness = ForDevice().Get(kBubbleRoundness);
}

int BubblePercent() {
	return BubbleRoundness;
}

int AdjustBubbleRadius(int radius) {
	return BubbleRoundness
		? std::max(1, radius * BubbleRoundness / 100)
		: radius;
}

std::optional<int> AvatarRadius(int size, Ui::PeerUserpicShape shape) {
	return AyuUserpic::ShouldOverrideShape(shape)
		? std::optional(AyuUserpic::ComputeRadius(size))
		: std::nullopt;
}

Ui::PeerUserpicShape ResolvedAvatarShape(
		Ui::PeerUserpicShape shape,
		PeerData *peer) {
	return (shape == Ui::PeerUserpicShape::Auto && peer)
		? peer->userpicShape() : shape;
}

QImage RoundedAvatarFrame(
		Media::Streaming::Instance &stream,
		int size,
		int radius,
		std::array<QImage, 4> &corners) {
	const auto ratio = style::DevicePixelRatio();
	auto request = Media::Streaming::FrameRequest();
	request.outer = request.resize = QSize(size, size) * ratio;
	if (corners[0].width() != radius * ratio) {
		corners = Images::CornersMask(radius);
	}
	request.rounding = Images::CornersMaskRef(corners);
	return stream.frame(request);
}

} // namespace Nagram::Interface
