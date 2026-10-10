#pragma once

#include <QtCore/QByteArray>

#include <memory>
#include <vector>

namespace Main {
class Session;
} // namespace Main

namespace Nagram::Sync {

enum class BackendError {
	None,
	Search,
	Download,
	TooLarge,
	Upload,
	Send,
};

struct Fetched {
	BackendError error = BackendError::None;
	bool found = false;
	quint64 id = 0;
	QByteArray data;
	std::vector<quint64> all;
};

struct Uploaded {
	BackendError error = BackendError::None;
	quint64 id = 0;
};

class Backend {
public:
	virtual ~Backend() = default;

	virtual void fetch(quint64 knownId, Fn<void(Fetched)> done) = 0;
	virtual void upload(
		const QByteArray &data,
		Fn<void(Uploaded)> done) = 0;
	virtual void remove(const std::vector<quint64> &ids) = 0;
	virtual void cancel() = 0;

};

[[nodiscard]] std::unique_ptr<Backend> MakeSavedMessagesBackend(
	not_null<Main::Session*> session);

} // namespace Nagram::Sync
