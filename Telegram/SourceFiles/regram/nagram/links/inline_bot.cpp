#include "regram/nagram/links/inline_bot.h"

#include "regram/nagram/links/inline_rules.h"
#include "chat_helpers/message_field.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "ui/widgets/fields/input_field.h"

namespace Nagram::Links {
namespace {

struct State {
	QByteArray rules;
	InlineMatcher matcher;
	QString text;
	QString username;
	bool matched = false;
};

[[nodiscard]] QString AutomaticUsername(const TextWithTags &text) {
	static auto state = State();
	const auto rules = ForDevice().Get(kInlineBotRules);
	if (state.rules != rules) {
		state = { .rules = rules, .matcher = CompileInlineRules(rules) };
	}
	if (!state.matched || state.text != text.text) {
		const auto match = AutomaticInlineBot(
			true,
			state.matcher,
			text.text,
			false);
		if (!match.error.isEmpty()) {
			LOG(("Nagram inline bot: %1.").arg(match.error));
		}
		state.text = text.text;
		state.username = match.username;
		state.matched = true;
	}
	return text.tags.isEmpty() ? state.username : QString();
}

} // namespace

bool AutoInlineBotEnabled() {
	return ForDevice().Get(kAutoInlineBot);
}

bool AutomaticInlineQuery(not_null<const Ui::InputField*> field) {
	return AutoInlineBotEnabled()
		&& !AutomaticUsername(field->getTextWithTags()).isEmpty();
}

void FillAutomaticInlineQuery(
		not_null<Main::Session*> session,
		const TextWithTags &text,
		InlineBotQuery &result) {
	if (!AutoInlineBotEnabled()) {
		return;
	}
	const auto username = AutomaticUsername(text);
	if (username.isEmpty()) {
		return;
	}
	const auto peer = session->data().peerByUsername(username);
	const auto user = peer ? peer->asUser() : nullptr;
	if (peer && (!user
		|| !user->isBot()
		|| user->botInfo->inlinePlaceholder.isEmpty())) {
		return;
	}
	result.username = username;
	result.query = text.text.trimmed();
	result.bot = user;
	result.lookingUpBot = !user;
}

} // namespace Nagram::Links
