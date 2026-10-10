#include "regram/nagram/media/audio.h"

#include "regram/nagram/media/backend_options.h"

namespace Nagram::Media {

int VoiceRecordBitrate(int fallback) {
	return VoiceRecordBitrate(ForDevice(), fallback);
}

bool GroupCallRawAudio() {
	return ForDevice().Get(kGroupCallRawAudio);
}

} // namespace Nagram::Media
