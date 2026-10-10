#pragma once

#include <QtCore/QtGlobal>

#include <optional>
#include <vector>

namespace Nagram {

inline constexpr auto kTranscribeBatchLimit = 20;
inline constexpr auto kTranscribeNetworkFailures = 3;
inline constexpr auto kTranscribeMaxBytes = 24 * 1024 * 1024;

enum class TranscribeSkip {
	None,
	NotAudio,
	Expiring,
	Cached,
	NotDownloaded,
	TooLarge,
};

struct TranscribeCandidate {
	bool audio = false;
	bool expiring = false;
	bool cached = false;
	bool downloaded = false;
	qint64 size = 0;
};

struct TranscribePlan {
	std::vector<int> accepted;
	int skipped = 0;
};

[[nodiscard]] TranscribeSkip ClassifyTranscription(
	const TranscribeCandidate &candidate);
[[nodiscard]] TranscribePlan PlanTranscription(
	const std::vector<TranscribeCandidate> &candidates);

enum class TranscribeOutcome {
	Done,
	Failed,
	Network,
	Fatal,
	Changed,
};

enum class TranscribeStop {
	None,
	Finished,
	Cancelled,
	Network,
	Fatal,
	Changed,
};

class TranscriptionQueue final {
public:
	explicit TranscriptionQueue(int total = 0);

	[[nodiscard]] std::optional<int> current() const;
	void report(TranscribeOutcome outcome);
	void cancel();

	[[nodiscard]] TranscribeStop stopped() const {
		return _stopped;
	}
	[[nodiscard]] int total() const {
		return _total;
	}
	[[nodiscard]] int done() const {
		return _done;
	}
	[[nodiscard]] int failed() const {
		return _failed;
	}
	[[nodiscard]] int left() const {
		return _total - _done - _failed;
	}

private:
	int _total = 0;
	int _index = 0;
	int _done = 0;
	int _failed = 0;
	int _networkFailures = 0;
	TranscribeStop _stopped = TranscribeStop::None;

};

} // namespace Nagram
