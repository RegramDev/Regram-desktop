#include "regram/nagram/services/transcription_queue.h"

namespace Nagram {

TranscribeSkip ClassifyTranscription(const TranscribeCandidate &candidate) {
	return !candidate.audio
		? TranscribeSkip::NotAudio
		: candidate.expiring
		? TranscribeSkip::Expiring
		: candidate.cached
		? TranscribeSkip::Cached
		: (candidate.size <= 0 || candidate.size > kTranscribeMaxBytes)
		? TranscribeSkip::TooLarge
		: TranscribeSkip::None;
}

TranscribePlan PlanTranscription(
		const std::vector<TranscribeCandidate> &candidates) {
	auto result = TranscribePlan();
	for (auto i = 0; i != int(candidates.size()); ++i) {
		const auto skip = ClassifyTranscription(candidates[i]);
		if (skip == TranscribeSkip::NotAudio) {
			continue;
		} else if (skip != TranscribeSkip::None
			|| result.accepted.size() == kTranscribeBatchLimit) {
			++result.skipped;
		} else {
			result.accepted.push_back(i);
		}
	}
	return result;
}

TranscriptionQueue::TranscriptionQueue(int total)
: _total(total)
, _stopped(total > 0 ? TranscribeStop::None : TranscribeStop::Finished) {
}

std::optional<int> TranscriptionQueue::current() const {
	return (_stopped == TranscribeStop::None)
		? std::make_optional(_index)
		: std::nullopt;
}

void TranscriptionQueue::report(TranscribeOutcome outcome) {
	if (_stopped != TranscribeStop::None) {
		return;
	}
	++_index;
	if (outcome == TranscribeOutcome::Done) {
		++_done;
		_networkFailures = 0;
	} else {
		++_failed;
		_networkFailures = (outcome == TranscribeOutcome::Network)
			? (_networkFailures + 1)
			: 0;
	}
	_stopped = (outcome == TranscribeOutcome::Fatal)
		? TranscribeStop::Fatal
		: (outcome == TranscribeOutcome::Changed)
		? TranscribeStop::Changed
		: (_networkFailures == kTranscribeNetworkFailures)
		? TranscribeStop::Network
		: (_index == _total)
		? TranscribeStop::Finished
		: TranscribeStop::None;
}

void TranscriptionQueue::cancel() {
	if (_stopped == TranscribeStop::None) {
		_stopped = TranscribeStop::Cancelled;
	}
}

} // namespace Nagram
