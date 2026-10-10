#include "regram/nagram/media/url_template.h"

#include "base/basic_types.h"

#include <QtCore/QUrl>

#include <optional>
#include <vector>

namespace Nagram::Media {
namespace {

enum class Part { Text, Artist, Title };

struct Piece {
	Part part = Part::Text;
	QString text;
};

struct Parsed {
	std::vector<Piece> pieces;
	QUrl probe;
	bool hasTitle = false;
	bool hasPlaceholders = false;
};

[[nodiscard]] bool IsLoopback(const QString &host) {
	if (host == u"localhost"_q || host == u"::1"_q) {
		return true;
	}
	const auto parts = host.split(u'.');
	if (parts.size() != 4 || parts.front() != u"127"_q) {
		return false;
	}
	for (const auto &part : parts) {
		auto ok = false;
		const auto value = part.toInt(&ok);
		if (!ok || value < 0 || value > 255) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::optional<Parsed> Parse(const QString &address) {
	if (address.isEmpty() || address.size() > kMaxCoverUrlLength) {
		return std::nullopt;
	}
	for (const auto &ch : address) {
		if (ch.isSpace() || !ch.isPrint()) {
			return std::nullopt;
		}
	}
	const auto schemeEnd = address.indexOf(u"://"_q);
	if (schemeEnd <= 0) {
		return std::nullopt;
	}
	auto authorityEnd = address.size();
	for (auto i = schemeEnd + 3; i != address.size(); ++i) {
		if (address[i] == u'/' || address[i] == u'?') {
			authorityEnd = i;
			break;
		}
	}
	auto result = Parsed();
	auto probe = QString();
	auto from = 0;
	while (from < address.size()) {
		const auto open = address.indexOf(u'{', from);
		const auto text = address.mid(
			from,
			(open < 0) ? -1 : (open - from));
		if (text.contains(u'}')) {
			return std::nullopt;
		} else if (!text.isEmpty()) {
			result.pieces.push_back({ Part::Text, text });
			probe += text;
		}
		if (open < 0) {
			break;
		}
		const auto close = address.indexOf(u'}', open);
		if (close < 0 || open < authorityEnd) {
			return std::nullopt;
		}
		const auto name = address.mid(open + 1, close - open - 1);
		if (name == u"artist"_q) {
			result.pieces.push_back({ Part::Artist });
		} else if (name == u"title"_q) {
			result.pieces.push_back({ Part::Title });
			result.hasTitle = true;
		} else {
			return std::nullopt;
		}
		result.hasPlaceholders = true;
		probe += u'x';
		from = close + 1;
	}
	if (result.hasPlaceholders && !result.hasTitle) {
		return std::nullopt;
	}
	result.probe = QUrl(probe, QUrl::StrictMode);
	const auto &url = result.probe;
	const auto scheme = url.scheme();
	const auto host = url.host();
	const auto allowed = (scheme == u"https"_q)
		|| (scheme == u"http"_q && IsLoopback(host));
	if (!url.isValid()
		|| !allowed
		|| host.isEmpty()
		|| !url.userInfo().isEmpty()
		|| url.hasFragment()
		|| (url.port() != -1 && (url.port() <= 0 || url.port() > 65535))) {
		return std::nullopt;
	}
	return result;
}

[[nodiscard]] QString Encode(const QString &value) {
	return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

} // namespace

bool ValidCoverAddress(const QString &address) {
	return Parse(address).has_value();
}

bool ValidCoverUrl(const QString &value) {
	return value.isEmpty() || ValidCoverAddress(value);
}

QString CoverUrlHost(const QString &address) {
	const auto parsed = Parse(address);
	return parsed ? parsed->probe.host() : QString();
}

QString ExpandCoverUrl(
		const QString &address,
		const QString &artist,
		const QString &title) {
	const auto parsed = Parse(address);
	if (!parsed) {
		return QString();
	} else if (!parsed->hasPlaceholders) {
		const auto query = artist.isEmpty()
			? title
			: (artist + u" - "_q + title);
		return address + Encode(query);
	}
	auto result = QString();
	for (const auto &piece : parsed->pieces) {
		result += (piece.part == Part::Artist)
			? Encode(artist)
			: (piece.part == Part::Title)
			? Encode(title)
			: piece.text;
	}
	return result;
}

} // namespace Nagram::Media
