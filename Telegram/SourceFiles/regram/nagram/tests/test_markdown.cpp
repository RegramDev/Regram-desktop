#include "regram/nagram/messages/markdown.h"

#include "base/basic_types.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Nagram::Markdown::Span;
using Nagram::Markdown::Style;

void Same(
		const QString &text,
		const std::vector<Span> &spans,
		const QString &expected,
		const char *message) {
	const auto result = Nagram::Markdown::Convert(text, spans);
	if (result != expected) {
		throw std::runtime_error(std::string("markdown ") + message
			+ ": got " + result.toStdString());
	}
}

} // namespace

void TestMarkdown() {
	using Nagram::Markdown::LinkTarget;

	Same(u"a*b [x] <y> |z| \\ ~` &amp; R&D"_q, {},
		u"a\\*b \\[x\\] \\<y> \\|z\\| \\\\ \\~\\` \\&amp; R&D"_q,
		"plain text escaped");
	Same(u"snake_case _x_ a_ _b"_q, {},
		u"snake_case \\_x\\_ a\\_ \\_b"_q, "underscores inside words kept");
	Same(u"# title\n> quote\n- item\n+ item\n1. one\n2) two\n---\n=="_q, {},
		u"\\# title\n\\> quote\n\\- item\n\\+ item\n1\\. one\n2\\) two\n\\---\n\\=="_q,
		"block markers at line start escaped");
	Same(u"a # b > c - d 1. e\n-5 +x 1.5 #"_q, {},
		u"a # b > c - d 1. e\n-5 +x 1.5 #"_q,
		"block markers inside a line kept");
	Same(u"  - item"_q, {}, u"  \\- item"_q, "indented list marker escaped");

	Same(u"Hello world"_q, { { Style::Bold, 0, 5 } },
		u"**Hello** world"_q, "bold");
	Same(u"a bold  b"_q, { { Style::Bold, 1, 7 } },
		u"a **bold**  b"_q, "spaces moved out of the marks");
	Same(u"a   b"_q, { { Style::Bold, 1, 3 } },
		u"a   b"_q, "style over spaces only dropped");
	Same(u"say hi now"_q, { { Style::Italic, 4, 2 } },
		u"say _hi_ now"_q, "italic word");
	Same(u"unbelievable"_q, { { Style::Italic, 2, 6 } },
		u"un*believ*able"_q, "italic inside a word");
	Same(u"bold italic"_q, {
		{ Style::Bold, 0, 11 },
		{ Style::Italic, 5, 6 },
	}, u"**bold _italic_**"_q, "nested styles");
	Same(u"abcdef"_q, {
		{ Style::Bold, 0, 4 },
		{ Style::Strike, 2, 4 },
	}, u"**ab~~cd~~**~~ef~~"_q, "overlapping styles stay balanced");
	Same(u"one\ntwo\n\nthree"_q, { { Style::Bold, 0, 14 } },
		u"**one**\n**two**\n\n**three**"_q, "styles closed on every line");
	Same(u"# x"_q, { { Style::Bold, 0, 3 } },
		u"**# x**"_q, "marker behind a mark kept");
	Same(u"u s x"_q, {
		{ Style::Underline, 0, 1 },
		{ Style::Spoiler, 2, 1 },
		{ Style::Strike, 4, 1 },
	}, u"<u>u</u> ||s|| ~~x~~"_q, "underline, spoiler, strike");

	Same(u"a`b"_q, { { Style::Code, 0, 3 } },
		u"``a`b``"_q, "code fence longer than its backticks");
	Same(u"`x"_q, { { Style::Code, 0, 2 } },
		u"`` `x ``"_q, "code starting with a backtick");
	Same(u"*x*"_q, {
		{ Style::Bold, 0, 3 },
		{ Style::Code, 0, 3 },
	}, u"**`*x*`**"_q, "code verbatim inside a style");
	Same(u"ab*cd*ef"_q, {
		{ Style::Code, 2, 4 },
		{ Style::Bold, 0, 4 },
		{ Style::Italic, 4, 4 },
	}, u"**ab**`*cd*`_ef_"_q, "styles never split code");
	Same(u"https://a.com/x_y*z #tag_"_q, {
		{ Style::Verbatim, 0, 19 },
		{ Style::Verbatim, 20, 5 },
	}, u"https://a.com/x_y*z #tag_"_q, "links and hashtags verbatim");

	Same(u"site"_q, { { Style::Link, 0, 4, u"https://e.com/a b(c)"_q } },
		u"[site](https://e.com/a%20b%28c%29)"_q, "link target encoded");
	Same(u"[x]"_q, { { Style::Link, 0, 3, u"https://e.com"_q } },
		u"[\\[x\\]](https://e.com)"_q, "link text escaped");
	Same(u"click"_q, { { Style::Link, 0, 5, u"javascript:alert(1)"_q } },
		u"click"_q, "script link dropped");
	Same(u"click"_q, { { Style::Link, 0, 5, u"e.com"_q } },
		u"click"_q, "link without a scheme dropped");
	if (LinkTarget(u" HTTPS://e.com/\n<x>\"\\ "_q)
			!= u"HTTPS://e.com/%0A%3Cx%3E%22%5C"_q
		|| LinkTarget(u"tg://user?id=1"_q) != u"tg://user?id=1"_q
		|| !LinkTarget(u"data:text/html,x"_q).isEmpty()
		|| !LinkTarget(u"file:///etc/passwd"_q).isEmpty()) {
		throw std::runtime_error("markdown link targets");
	}

	Same(u"print(1)"_q, { { Style::Pre, 0, 8, u"python"_q } },
		u"```python\nprint(1)\n```"_q, "code block");
	Same(u"see:\ncode\nend"_q, { { Style::Pre, 5, 4 } },
		u"see:\n```\ncode\n```\nend"_q, "code block between text");
	Same(u"a code b"_q, { { Style::Pre, 2, 4 } },
		u"a \n```\ncode\n```\n b"_q, "code block inside a line");
	Same(u"a```b"_q, { { Style::Pre, 0, 5, u"c++ `bad`"_q } },
		u"````c++\na```b\n````"_q, "fence longer, language cleaned");
	Same(u"code\n\nafter"_q, { { Style::Pre, 0, 4 } },
		u"```\ncode\n```\n\nafter"_q, "blank line after a block kept");

	Same(u"quote line\nsecond"_q, { { Style::Quote, 0, 17 } },
		u"> quote line\n> second"_q, "quote");
	Same(u"q\nafter"_q, { { Style::Quote, 0, 1 } },
		u"> q\n\nafter"_q, "text after a quote leaves it");
	Same(u"before\nq"_q, { { Style::Quote, 7, 1 } },
		u"before\n> q"_q, "quote after text");
	Same(u"a\nb"_q, { { Style::Quote, 0, 2 }, { Style::Quote, 2, 1 } },
		u"> a\n\n> b"_q, "adjacent quotes stay apart");
	Same(u"# a\n\nbold"_q, {
		{ Style::Quote, 0, 9 },
		{ Style::Bold, 5, 4 },
	}, u"> \\# a\n>\n> **bold**"_q, "quote lines escaped and styled");

	Same(u"\U0001F600 bold"_q, { { Style::Bold, 3, 4 } },
		u"\U0001F600 **bold**"_q, "offsets count UTF-16 units");
	Same(u"abc"_q, {
		{ Style::Bold, 5, 100 },
		{ Style::Bold, -4, 2 },
		{ Style::Italic, 1, -1 },
		{ Style::Bold, 2, 2147483647 },
	}, u"ab**c**"_q, "spans outside the text clamped");
	std::cout << "PASS: Nagram Markdown copy" << std::endl;
}
