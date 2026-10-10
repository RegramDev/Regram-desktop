#include "regram/nagram/sync/model.h"

#include "regram/nagram/core/owned_json.h"

#include <QtCore/QCryptographicHash>

#include <algorithm>

namespace Nagram::Sync {
namespace {

constexpr auto kVersion = 1;
constexpr auto kAppLimit = 32;
constexpr auto kHashLength = 64;

[[nodiscard]] QString Kind() {
	return QStringLiteral("nagram-sync");
}

[[nodiscard]] std::optional<QJsonObject> ParseObject(const QByteArray &data) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(data, &error);
	return (error.error == QJsonParseError::NoError && document.isObject())
		? std::make_optional(document.object())
		: std::nullopt;
}

[[nodiscard]] std::optional<qint64> Integer(const QJsonValue &value) {
	if (!value.isDouble()) {
		return std::nullopt;
	}
	const auto number = value.toDouble();
	const auto integer = qint64(number);
	return (number >= 0. && number < 1e15 && double(integer) == number)
		? std::make_optional(integer)
		: std::nullopt;
}

[[nodiscard]] bool ValidHash(const QByteArray &hash) {
	if (hash.size() != kHashLength) {
		return false;
	}
	for (const auto ch : hash) {
		if (!(ch >= '0' && ch <= '9') && !(ch >= 'a' && ch <= 'f')) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] QByteArray HashObject(const QJsonObject &payload) {
	return QCryptographicHash::hash(
		QJsonDocument(payload).toJson(QJsonDocument::Compact),
		QCryptographicHash::Sha256).toHex();
}

[[nodiscard]] QStringList StateKeys() {
	return {
		QStringLiteral("version"),
		QStringLiteral("user"),
		QStringLiteral("messageId"),
		QStringLiteral("hash"),
		QStringLiteral("updatedAt"),
	};
}

} // namespace

QString FileName() {
	return u"regram-sync-v1.json"_q;
}

QString Caption() {
	return u"#regram_sync"_q;
}

QByteArray PayloadHash(const QByteArray &exported) {
	const auto payload = ParseObject(exported);
	return payload ? HashObject(*payload) : QByteArray();
}

QByteArray EncodeEnvelope(
		const QByteArray &exported,
		qint64 updatedAt,
		const QString &app) {
	const auto payload = ParseObject(exported);
	if (!payload) {
		return QByteArray();
	}
	return QJsonDocument(QJsonObject{
		{ QStringLiteral("version"), kVersion },
		{ QStringLiteral("kind"), Kind() },
		{ QStringLiteral("updatedAt"), double(updatedAt) },
		{ QStringLiteral("app"), app.left(kAppLimit) },
		{ QStringLiteral("payload"), *payload },
	}).toJson(QJsonDocument::Indented);
}

DecodedEnvelope DecodeEnvelope(const QByteArray &data) {
	if (data.size() > kMaxBackupBytes) {
		return { EnvelopeError::TooLarge };
	}
	const auto root = ParseObject(data);
	if (!root) {
		return { EnvelopeError::Damaged };
	}
	const auto kind = root->value(QStringLiteral("kind"));
	if (!kind.isString() || kind.toString() != Kind()) {
		return { EnvelopeError::Foreign };
	}
	const auto version = Integer(root->value(QStringLiteral("version")));
	if (!version || !*version) {
		return { EnvelopeError::Damaged };
	} else if (*version > kVersion) {
		return { EnvelopeError::Newer };
	}
	auto keys = QStringList{
		QStringLiteral("version"),
		QStringLiteral("kind"),
		QStringLiteral("updatedAt"),
		QStringLiteral("app"),
		QStringLiteral("payload"),
	};
	keys.sort();
	const auto updatedAt = Integer(root->value(QStringLiteral("updatedAt")));
	const auto app = root->value(QStringLiteral("app"));
	const auto payload = root->value(QStringLiteral("payload"));
	if (root->keys() != keys
		|| !updatedAt
		|| !app.isString()
		|| app.toString().size() > kAppLimit
		|| !payload.isObject()) {
		return { EnvelopeError::Damaged };
	}
	const auto object = payload.toObject();
	const auto format = Integer(object.value(QStringLiteral("version")));
	if (!format || !*format) {
		return { EnvelopeError::Damaged };
	} else if (*format > kVersion) {
		return { EnvelopeError::Newer };
	} else if (object.size() != 2
		|| !object.value(QStringLiteral("options")).isObject()) {
		return { EnvelopeError::Damaged };
	}
	return { EnvelopeError::None, {
		.updatedAt = *updatedAt,
		.app = app.toString(),
		.payload = QJsonDocument(object).toJson(QJsonDocument::Compact),
		.hash = HashObject(object),
	} };
}

Action Decide(
		const QByteArray &local,
		const QByteArray &synced,
		const QByteArray &remote) {
	if (remote.isEmpty()) {
		return Action::Upload;
	} else if (remote == local) {
		return Action::UpToDate;
	} else if (synced == remote) {
		return Action::Upload;
	} else if (synced == local) {
		return Action::Download;
	}
	return Action::Conflict;
}

std::optional<State> ParseState(const QByteArray &raw) {
	const auto object = ParseOwnedJson(raw, StateKeys());
	if (!object) {
		return std::nullopt;
	}
	const auto user = DecimalId(object->value(QStringLiteral("user")));
	const auto message = DecimalId(
		object->value(QStringLiteral("messageId")),
		true);
	const auto hash = object->value(QStringLiteral("hash"));
	const auto updatedAt = Integer(
		object->value(QStringLiteral("updatedAt")));
	if (!user || !message || !updatedAt
		|| !hash.isString()
		|| !ValidHash(hash.toString().toLatin1())
		|| hash.toString().size() != kHashLength) {
		return std::nullopt;
	}
	return State{
		.user = *user,
		.messageId = *message,
		.hash = hash.toString().toLatin1(),
		.updatedAt = *updatedAt,
	};
}

QByteArray SerializeState(const State &state) {
	auto object = NewOwnedJson(state.user);
	object.insert(
		QStringLiteral("messageId"),
		QString::number(state.messageId));
	object.insert(QStringLiteral("hash"), QString::fromLatin1(state.hash));
	object.insert(QStringLiteral("updatedAt"), double(state.updatedAt));
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool ValidState(const QByteArray &raw) {
	return raw.isEmpty() || ParseState(raw).has_value();
}

std::vector<quint64> SupersededBackups(
		std::vector<quint64> remote,
		quint64 known,
		quint64 uploaded) {
	if (known) {
		remote.push_back(known);
	}
	std::erase(remote, uploaded);
	// Deleting one message twice in a request destroys its item twice.
	std::sort(begin(remote), end(remote));
	remote.erase(std::unique(begin(remote), end(remote)), end(remote));
	return remote;
}

AutoPlan PlanAuto(
		qint64 now,
		qint64 lastAttempt,
		const QByteArray &local,
		const QByteArray &synced) {
	if (local.isEmpty() || local == synced) {
		return { AutoStep::Skip };
	}
	const auto passed = now - lastAttempt;
	if (lastAttempt > 0 && passed >= 0 && passed < kAutoIntervalSeconds) {
		return { AutoStep::Wait, kAutoIntervalSeconds - passed };
	}
	return { AutoStep::Run };
}

bool ValidAutoOwner(const QString &value) {
	return value.isEmpty() || DecimalId(QJsonValue(value)).has_value();
}

bool AutoEnabled(const QString &value, quint64 user) {
	return user && (DecimalId(QJsonValue(value)) == user);
}

QStringList LocalOnlyKeys(const Registry &registry) {
	auto result = QStringList();
	for (const auto &info : registry.All()) {
		if (info.flags & static_cast<unsigned>(Flag::LocalOnly)) {
			result.push_back(
				QString::fromUtf8(info.key.data(), info.key.size()));
		}
	}
	result.sort();
	return result;
}

} // namespace Nagram::Sync
