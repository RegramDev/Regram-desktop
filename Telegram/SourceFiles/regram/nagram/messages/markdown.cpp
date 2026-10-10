#include "regram/nagram/messages/markdown.h"

#include "base/basic_types.h"

#include <QtCore/QStringList>
#include <QtCore/QStringView>
#include <QtCore/QUrl>

#include <algorithm>

namespace Nagram::Markdown {
namespace {

constexpr auto kMaxLanguageLength = 32;
constexpr auto kMaxListDigits = 9;
constexpr auto kMaxReferenceLength = 32;

struct Range {
	Style style = Style::Verbatim;
	int from = 0;
	int till = 0;
	QString data;
};

struct Marks {
	QString open;
	QString close;
};

[[nodiscard]] bool Block(Style style) {
	return (style == Style::Pre) || (style == Style::Quote);
}

[[nodiscard]] bool Atomic(Style style) {
	return (style == Style::Code) || (style == Style::Verbatim);
}

[[nodiscard]] bool Word(QStringView text, int index) {
	return (index >= 0)
		&& (index < text.size())
		&& text[index].isLetterOrNumber();
}

[[nodiscard]] int LongestRun(QStringView text, QChar ch) {
	auto result = 0;
	auto current = 0;
	for (const auto c : text) {
		current = (c == ch) ? (current + 1) : 0;
		result = std::max(result, current);
	}
	return result;
}

[[nodiscard]] bool Reference(QStringView text, int from) {
	auto till = from;
	while (till < text.size()
		&& till - from < kMaxReferenceLength
		&& (text[till] == u'#' || Word(text, till))) {
		++till;
	}
	return (till > from) && (till < text.size()) && (text[till] == u';');
}

[[nodiscard]] bool Special(QStringView text, int index) {
	switch (text[index].unicode()) {
	case u'\\':
	case u'*':
	case u'`':
	case u'[':
	case u']':
	case u'<':
	case u'~':
	case u'|': return true;
	case u'_': return !Word(text, index - 1) || !Word(text, index + 1);
	case u'&': return Reference(text, index + 1);
	}
	return false;
}

[[nodiscard]] QString Escaped(
		QStringView text,
		int from,
		int till,
		int forced) {
	auto result = QString();
	result.reserve(till - from);
	for (auto i = from; i != till; ++i) {
		if (i == forced || Special(text, i)) {
			result.append(u'\\');
		}
		result.append(text[i]);
	}
	return result;
}

// The character that would start a heading, a quote, a list or a rule.
[[nodiscard]] int BlockMarker(
		QStringView text,
		int from,
		int till,
		int lineTill) {
	const auto ends = [&](int index) {
		return (index >= lineTill)
			|| (index < till && text[index].isSpace());
	};
	const auto next = [&](int index) {
		return (index < till) ? text[index] : QChar();
	};
	const auto ch = text[from];
	if (ch == u'#' || ch == u'>') {
		return from;
	} else if (ch == u'+') {
		return ends(from + 1) ? from : -1;
	} else if (ch == u'-' || ch == u'=') {
		return (ends(from + 1) || next(from + 1) == ch) ? from : -1;
	}
	auto digits = from;
	while (digits < till
		&& digits - from < kMaxListDigits
		&& text[digits] >= u'0'
		&& text[digits] <= u'9') {
		++digits;
	}
	const auto after = next(digits);
	return (digits > from
		&& (after == u'.' || after == u')')
		&& ends(digits + 1))
		? digits
		: -1;
}

[[nodiscard]] Marks MarksFor(QStringView text, const Range &range) {
	switch (range.style) {
	case Style::Bold: return { u"**"_q, u"**"_q };
	case Style::Italic: {
		const auto inside = Word(text, range.from - 1)
			|| Word(text, range.till);
		const auto mark = inside ? u"*"_q : u"_"_q;
		return { mark, mark };
	}
	case Style::Underline: return { u"<u>"_q, u"</u>"_q };
	case Style::Strike: return { u"~~"_q, u"~~"_q };
	case Style::Spoiler: return { u"||"_q, u"||"_q };
	case Style::Link: return { u"["_q, u"]("_q + range.data + u')' };
	case Style::Code: {
		const auto content = text.mid(range.from, range.till - range.from);
		const auto fence = QString(LongestRun(content, u'`') + 1, u'`');
		const auto spaced = content.front().isSpace()
			&& content.back().isSpace()
			&& !content.trimmed().isEmpty();
		const auto pad = (spaced
			|| content.front() == u'`'
			|| content.back() == u'`')
			? u" "_q
			: QString();
		return { fence + pad, pad + fence };
	}
	default: return {};
	}
}

[[nodiscard]] std::vector<Range> InlineRanges(
		QStringView text,
		int from,
		int till,
		const std::vector<Range> &ranges) {
	auto result = std::vector<Range>();
	auto styled = std::vector<Range>();
	for (const auto &range : ranges) {
		auto clipped = range;
		clipped.from = std::max(range.from, from);
		clipped.till = std::min(range.till, till);
		if (Block(range.style) || clipped.from >= clipped.till) {
			continue;
		} else if (!Atomic(range.style)) {
			styled.push_back(std::move(clipped));
		} else if (result.empty() || result.back().till <= clipped.from) {
			result.push_back(std::move(clipped));
		}
	}
	const auto atomic = result;
	const auto edge = [&](int index) {
		return std::ranges::any_of(atomic, [&](const Range &range) {
			return (range.from == index) || (range.till == index);
		});
	};
	for (auto &range : styled) {
		for (const auto &inner : atomic) {
			const auto same = (inner.from == range.from)
				&& (inner.till == range.till);
			if (!same && inner.from <= range.from && range.till <= inner.till) {
				range.till = range.from;
				break;
			} else if (inner.from < range.from && range.from < inner.till) {
				range.from = inner.till;
			}
			if (inner.from < range.till && range.till < inner.till) {
				range.till = inner.from;
			}
		}
		while (range.from < range.till
			&& text[range.from].isSpace()
			&& !edge(range.from)) {
			++range.from;
		}
		while (range.from < range.till
			&& text[range.till - 1].isSpace()
			&& !edge(range.till)) {
			--range.till;
		}
		if (range.from < range.till) {
			result.push_back(std::move(range));
		}
	}
	return result;
}

[[nodiscard]] QString RenderLine(
		QStringView text,
		int from,
		int till,
		const std::vector<Range> &ranges) {
	const auto list = InlineRanges(text, from, till, ranges);
	auto marks = std::vector<Marks>();
	auto points = std::vector<int>{ from, till };
	for (const auto &range : list) {
		marks.push_back(MarksFor(text, range));
		points.push_back(range.from);
		points.push_back(range.till);
	}
	std::ranges::sort(points);
	points.erase(std::ranges::unique(points).begin(), points.end());

	auto result = QString();
	auto open = std::vector<int>();
	auto started = false;
	const auto close = [&](int keep) {
		while (int(open.size()) > keep) {
			result += marks[open.back()].close;
			open.pop_back();
		}
	};
	for (auto i = 0; i + 1 < int(points.size()); ++i) {
		const auto a = points[i];
		const auto b = points[i + 1];
		auto active = std::vector<int>();
		auto raw = false;
		for (auto index = 0; index != int(list.size()); ++index) {
			if (list[index].from <= a && list[index].till >= b) {
				active.push_back(index);
				raw = raw || Atomic(list[index].style);
			}
		}
		auto keep = 0;
		while (keep < int(open.size())
			&& std::ranges::find(active, open[keep]) != active.end()) {
			++keep;
		}
		close(keep);
		std::erase_if(active, [&](int index) {
			return std::ranges::find(open, index) != open.end();
		});
		std::ranges::sort(active, [&](int first, int second) {
			const auto &one = list[first];
			const auto &two = list[second];
			return (Atomic(one.style) != Atomic(two.style))
				? Atomic(two.style)
				: (one.till != two.till)
				? (one.till > two.till)
				: (one.style < two.style);
		});
		for (const auto index : active) {
			result += marks[index].open;
			open.push_back(index);
		}
		started = started || !active.empty();
		if (raw) {
			result += text.mid(a, b - a);
			continue;
		}
		auto forced = -1;
		if (!started) {
			auto first = a;
			while (first < b && text[first].isSpace()) {
				++first;
			}
			if (first < b) {
				forced = BlockMarker(text, first, b, till);
				started = true;
			}
		}
		result += Escaped(text, a, b, forced);
	}
	close(0);
	return result;
}

[[nodiscard]] QStringList RenderLines(
		QStringView text,
		int from,
		int till,
		const std::vector<Range> &ranges) {
	auto result = QStringList();
	auto start = from;
	for (auto i = from; i <= till; ++i) {
		if (i == till || text[i] == u'\n') {
			result.push_back(RenderLine(text, start, i, ranges));
			start = i + 1;
		}
	}
	return result;
}

[[nodiscard]] QString Language(const QString &data) {
	auto result = QString();
	for (const auto ch : data) {
		const auto allowed = (ch.unicode() < 128 && ch.isLetterOrNumber())
			|| u"_+#.-"_q.contains(ch);
		if (!allowed || result.size() == kMaxLanguageLength) {
			break;
		}
		result.append(ch);
	}
	return result;
}

[[nodiscard]] QString Fenced(QStringView content, const QString &language) {
	const auto fence = QString(
		std::max(3, LongestRun(content, u'`') + 1),
		u'`');
	return fence + Language(language) + u'\n'
		+ content.toString()
		+ (content.endsWith(u'\n') ? QString() : u"\n"_q)
		+ fence;
}

[[nodiscard]] QString Quoted(
		QStringView text,
		int from,
		int till,
		const std::vector<Range> &ranges) {
	while (till > from && text[till - 1] == u'\n') {
		--till;
	}
	auto lines = RenderLines(text, from, till, ranges);
	for (auto &line : lines) {
		line = line.isEmpty() ? u">"_q : (u"> "_q + line);
	}
	return lines.join(u'\n');
}

[[nodiscard]] int Newlines(const QString &text, bool leading) {
	auto result = 0;
	const auto size = int(text.size());
	while (result < size
		&& text[leading ? result : (size - 1 - result)] == u'\n') {
		++result;
	}
	return result;
}

} // namespace

QString LinkTarget(const QString &url) {
	const auto trimmed = url.trimmed();
	const auto scheme = trimmed.left(trimmed.indexOf(u':')).toLower();
	const auto allowed = { u"http"_q, u"https"_q, u"tg"_q, u"mailto"_q };
	if (std::ranges::find(allowed, scheme) == allowed.end()) {
		return QString();
	}
	auto result = QString();
	result.reserve(trimmed.size());
	for (const auto ch : trimmed) {
		const auto unsafe = (ch.unicode() <= 0x20)
			|| (ch.unicode() == 0x7F)
			|| ch.isSpace()
			|| u"()<>\\\""_q.contains(ch);
		if (unsafe) {
			result += QString::fromLatin1(QUrl::toPercentEncoding(QString(ch)));
		} else {
			result += ch;
		}
	}
	return result;
}

QString Convert(const QString &text, const std::vector<Span> &spans) {
	const auto size = int(text.size());
	auto ranges = std::vector<Range>();
	for (const auto &span : spans) {
		const auto from = std::clamp(span.from, 0, size);
		const auto till = int(std::clamp(
			qint64(span.from) + span.length,
			qint64(from),
			qint64(size)));
		const auto link = (span.style == Style::Link);
		auto data = link ? LinkTarget(span.data) : span.data;
		if (from < till && (!link || !data.isEmpty())) {
			ranges.push_back({ span.style, from, till, std::move(data) });
		}
	}
	std::ranges::stable_sort(ranges, {}, &Range::from);

	auto result = QString();
	auto pending = 0;
	const auto append = [&](const QString &chunk, int wanted) {
		const auto have = Newlines(result, false) + Newlines(chunk, true);
		const auto need = std::max(pending, wanted);
		if (!result.isEmpty() && have < need) {
			result += QString(need - have, u'\n');
		}
		result += chunk;
	};
	const auto plain = [&](int from, int till) {
		if (from < till) {
			append(RenderLines(text, from, till, ranges).join(u'\n'), 0);
			pending = 0;
		}
	};
	auto position = 0;
	for (const auto &block : ranges) {
		if (!Block(block.style) || block.from < position) {
			continue;
		}
		plain(position, block.from);
		const auto pre = (block.style == Style::Pre);
		append(pre
			? Fenced(
				QStringView(text).mid(block.from, block.till - block.from),
				block.data)
			: Quoted(text, block.from, block.till, ranges), 1);
		pending = pre ? 1 : 2;
		position = block.till;
	}
	plain(position, size);
	return result;
}

} // namespace Nagram::Markdown
