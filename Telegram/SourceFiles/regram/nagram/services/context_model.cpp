#include "regram/nagram/services/context_model.h"

#include <algorithm>

namespace Nagram {

QString TruncateContext(const QString &text, int limit) {
	if (text.size() <= limit) {
		return text;
	}
	const auto cut = (limit > 0 && text[limit - 1].isHighSurrogate())
		? (limit - 1)
		: limit;
	return text.left(cut);
}

QStringList SelectContext(
		const std::vector<ContextCandidate> &before,
		qint64 topic,
		bool restricted) {
	if (restricted) {
		return {};
	}
	auto result = QStringList();
	auto total = 0;
	for (auto i = before.rbegin(); i != before.rend(); ++i) {
		if (!i->regular || i->hidden || (i->topic != topic)) {
			continue;
		}
		const auto text = TruncateContext(i->text.trimmed(), kContextEach);
		if (text.isEmpty()) {
			continue;
		} else if (i->restricted) {
			return {};
		} else if (total + text.size() > kContextTotal) {
			break;
		}
		total += text.size();
		result.push_back(text);
		if (result.size() == kContextMessages) {
			break;
		}
	}
	std::reverse(result.begin(), result.end());
	return result;
}

} // namespace Nagram
