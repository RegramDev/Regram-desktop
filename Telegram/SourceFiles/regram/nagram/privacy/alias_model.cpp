#include "regram/nagram/privacy/alias.h"

#include "data/data_peer_id.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram::Privacy {
namespace {

constexpr auto kMaximumAliases = 1000;
constexpr auto kMaximumAliasLength = 96;

} // namespace

bool ValidAlias(const QString &value) {
	return value.size() <= kMaximumAliasLength
		&& value == value.trimmed()
		&& QString::fromUtf8(value.toUtf8()) == value
		&& ranges::none_of(value, [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		});
}

std::optional<PeerAliases> ParseAliases(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return PeerAliases();
	} else if (raw.size() > 512 * 1024) {
		return std::nullopt;
	}
	const auto document = QJsonDocument::fromJson(raw);
	const auto object = document.object();
	const auto names = object.value(u"names"_q);
	if (!document.isObject() || object.size() != 2
		|| object.value(u"version"_q) != QJsonValue(1)
		|| !names.isObject()
		|| names.toObject().size() > kMaximumAliases) {
		return std::nullopt;
	}
	auto result = PeerAliases();
	const auto entries = names.toObject();
	for (auto i = entries.begin(); i != entries.end(); ++i) {
		auto ok = false;
		const auto serialized = i.key().toULongLong(&ok);
		const auto id = DeserializePeerId(serialized);
		const auto value = i.value().toString();
		if (!ok || !id
			|| (!peerIsUser(id) && !peerIsChat(id) && !peerIsChannel(id))
			|| peerToBareMTPInt(id).v <= 0
			|| QString::number(SerializePeerId(id)) != i.key()
			|| !i.value().isString() || value.isEmpty()
			|| !ValidAlias(value)) {
			return std::nullopt;
		}
		result.emplace(id, value);
	}
	return result;
}

QByteArray SerializeAliases(const PeerAliases &aliases) {
	if (aliases.empty()) {
		return {};
	}
	auto names = QJsonObject();
	for (const auto &[id, value] : aliases) {
		names.insert(QString::number(SerializePeerId(id)), value);
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"names"_q, names },
	}).toJson(QJsonDocument::Compact);
}

} // namespace Nagram::Privacy
