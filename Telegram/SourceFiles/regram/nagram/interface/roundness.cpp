#include "regram/nagram/interface/roundness.h"

#include "regram/nagram/interface/options.h"
#include "data/data_peer.h"
#include "media/streaming/media_streaming_instance.h"
#include "ui/image/image_prepare.h"
#include "ui/style/style_core.h"

#include <algorithm>

namespace Nagram::Interface {
namespace {

int BubbleRoundness = 0;
int AvatarRoundness = 0;
bool UniformAvatarShapes = false;

} // namespace

void StartRoundness() {
	BubbleRoundness = ForDevice().Get(kBubbleRoundness);
	AvatarRoundness = ForDevice().Get(kAvatarRoundness);
	UniformAvatarShapes = ForDevice().Get(kUniformAvatarShapes);
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
	if (!AvatarRoundness
		|| (!UniformAvatarShapes
			&& (shape == Ui::PeerUserpicShape::Forum
				|| shape == Ui::PeerUserpicShape::Monoforum))) {
		return std::nullopt;
	}
	return std::max(1, size * AvatarRoundness / 200);
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
