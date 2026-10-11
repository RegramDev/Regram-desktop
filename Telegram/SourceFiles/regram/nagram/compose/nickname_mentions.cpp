#include "regram/nagram/compose/nickname_mentions.h"

#include "base/timer.h"
#include "chat_helpers/message_field.h"
#include "crl/crl_on_main.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "regram/nagram/compose/options.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"

#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QTextDocument>
#include <QtWidgets/QTextEdit>

#include <algorithm>

namespace Nagram::Compose {
namespace {

constexpr auto kTypingDelay = crl::time(650);
constexpr auto kLookupTimeout = crl::time(4000);
constexpr auto kCacheLifetime = crl::time(10000);
constexpr auto kMaxNames = 16;
constexpr auto kMaxDraftLength = 1024 * 1024;

struct Candidate {
	int offset = 0;
	int length = 0;
	QString username;
};

struct CachedUser {
	UserData *user = nullptr;
	crl::time resolved = 0;
};

bool ProtectedMentionSpan(EntityType type) {
	switch (type) {
	case EntityType::Bold:
	case EntityType::Semibold:
	case EntityType::Italic:
	case EntityType::Underline:
	case EntityType::StrikeOut:
	case EntityType::Spoiler:
	case EntityType::Blockquote:
	case EntityType::Subscript:
	case EntityType::Superscript:
	case EntityType::Marked:
	case EntityType::Colorized:
	case EntityType::Mention:
		return false;
	default:
		return true;
	}
}

class NicknameMentions final : public QObject {
public:
	NicknameMentions(
		not_null<Ui::InputField*> field,
		not_null<Main::Session*> session);

private:
	bool eventFilter(QObject *object, QEvent *event) override;
	void schedule();
	void resolve();
	void received(
		QString username,
		UserData *user,
		uint64 generation,
		bool cached = false);
	void apply();
	void cancelRequests();
	[[nodiscard]] bool current() const;

	const not_null<Ui::InputField*> _field;
	const not_null<Main::Session*> _session;
	MTP::Sender _api;
	base::Timer _typing;
	base::Timer _timeout;
	TextWithTags _snapshot;
	std::vector<Candidate> _candidates;
	QHash<QString, UserData*> _resolved;
	QHash<QString, CachedUser> _cache;
	std::vector<mtpRequestId> _requests;
	uint64 _generation = 0;
	int _revision = 0;
	int _position = 0;
	int _anchor = 0;
	int _textPosition = 0;
	int _pending = 0;
	bool _composing = false;
	bool _applying = false;
	rpl::lifetime _lifetime;

};

NicknameMentions::NicknameMentions(
		not_null<Ui::InputField*> field,
		not_null<Main::Session*> session)
: QObject(field)
, _field(field)
, _session(session)
, _api(&session->mtp())
, _typing([=] { resolve(); })
, _timeout([=] { apply(); }) {
	session->lifetime().add(crl::guard(this, [=] { delete this; }));
	const auto edit = field->rawTextEdit();
	edit->installEventFilter(this);
	QObject::connect(edit, &QTextEdit::cursorPositionChanged,
		this, [=] { schedule(); });
	QObject::connect(edit, &QTextEdit::selectionChanged,
		this, [=] { schedule(); });
	rpl::merge(field->changes(), field->focusedChanges() | rpl::to_empty)
	| rpl::on_next([=] { schedule(); }, _lifetime);
	ForDevice().Value(kNicknameMentions)
	| rpl::on_next([=] { schedule(); }, _lifetime);
}

bool NicknameMentions::eventFilter(QObject *object, QEvent *event) {
	if (event->type() == QEvent::InputMethod) {
		const auto input = static_cast<QInputMethodEvent*>(event);
		_composing = !input->preeditString().isEmpty();
		schedule();
	}
	return QObject::eventFilter(object, event);
}

void NicknameMentions::cancelRequests() {
	for (const auto id : base::take(_requests)) {
		_api.request(id).cancel();
	}
	_timeout.cancel();
	_pending = 0;
}

void NicknameMentions::schedule() {
	if (_applying) {
		return;
	}
	++_generation;
	_typing.cancel();
	cancelRequests();
	if (!ForDevice().Get(kNicknameMentions) || _composing
		|| !_field->hasFocus() || _field->rawTextEdit()->isReadOnly()
		|| _field->textCursor().hasSelection()) {
		return;
	}
	_typing.callOnce(kTypingDelay);
}

void NicknameMentions::resolve() {
	if (!ForDevice().Get(kNicknameMentions) || _composing || !_field->hasFocus()) {
		return;
	}
	_snapshot = _field->getTextWithTags();
	if (_snapshot.text.size() > kMaxDraftLength) {
		return;
	}
	const auto cursor = _field->textCursor();
	if (cursor.hasSelection()) {
		return;
	}
	_position = cursor.position();
	_anchor = cursor.anchor();
	_textPosition = int(_field->getTextWithTagsPart(0, _position).text.size());
	_revision = _field->rawTextEdit()->document()->revision();
	const auto parsed = TextUtilities::ParseEntities(_snapshot.text,
		TextParseLinks | TextParseMentions | TextParseBotCommands);
	auto entities = TextUtilities::ConvertTextTagsToEntities(_snapshot.tags);
	entities.append(parsed.entities);
	_candidates.clear();
	_resolved.clear();
	auto names = QStringList();
	for (const auto &entity : parsed.entities) {
		const auto start = entity.offset();
		const auto end = start + entity.length();
		if (entity.type() != EntityType::Mention || entity.length() < 2
			|| !entity.validForText(_snapshot.text.size())
			|| (_textPosition > start && _textPosition < end)) {
			continue;
		}
		if (ranges::any_of(entities, [&](const EntityInText &other) {
			return ProtectedMentionSpan(other.type())
				&& other.offset() < end
				&& other.offset() + other.length() > start;
		})) {
			continue;
		}
		const auto name = _snapshot.text.mid(start + 1, entity.length() - 1).toLower();
		if (!names.contains(name)) {
			if (names.size() == kMaxNames) {
				continue;
			}
			names.push_back(name);
		}
		_candidates.push_back({ start, entity.length(), name });
	}
	if (names.isEmpty()) {
		return;
	}
	_pending = int(names.size());
	const auto generation = _generation;
	for (const auto &name : names) {
		const auto cached = _cache.constFind(name);
		if (cached != _cache.cend() && crl::now() - cached->resolved < kCacheLifetime) {
			received(name, cached->user, generation, true);
			continue;
		}
		_requests.push_back(_api.request(MTPcontacts_ResolveUsername(
			MTP_flags(0),
			MTP_string(name),
			MTP_string()
		)).done(crl::guard(this, [=](const MTPcontacts_ResolvedPeer &result) {
			const auto &data = result.data();
			_session->data().processUsers(data.vusers());
			_session->data().processChats(data.vchats());
			const auto peer = _session->data().peerLoaded(peerFromMTP(data.vpeer()));
			const auto user = peer ? peer->asUser() : nullptr;
			received(name, user, generation);
		})).fail(crl::guard(this, [=](const MTP::Error &) {
			received(name, nullptr, generation);
		})).send());
	}
	if (_pending) {
		_timeout.callOnce(kLookupTimeout);
	}
}

bool NicknameMentions::current() const {
	const auto cursor = _field->textCursor();
	return ForDevice().Get(kNicknameMentions) && !_composing && _field->hasFocus()
		&& _field->rawTextEdit()->document()->revision() == _revision
		&& _field->getTextWithTags() == _snapshot
		&& cursor.position() == _position && cursor.anchor() == _anchor;
}

void NicknameMentions::received(
		QString username,
		UserData *user,
		uint64 generation,
		bool cached) {
	if (generation != _generation || !_pending) {
		return;
	}
	if (user && ranges::any_of(user->usernames(), [&](const QString &alias) {
		return alias.compare(username, Qt::CaseInsensitive) == 0;
	})) {
		_resolved.insert(username, user);
		if (!cached) {
			if (_cache.size() >= kMaxNames) {
				_cache.clear();
			}
			_cache.insert(username, { user, crl::now() });
		}
	}
	if (!--_pending) {
		apply();
	}
}

void NicknameMentions::apply() {
	const auto valid = current();
	++_generation;
	cancelRequests();
	if (!valid || _resolved.isEmpty()) {
		return;
	}
	auto updated = _snapshot;
	auto caret = _textPosition;
	std::sort(_candidates.begin(), _candidates.end(), [](const Candidate &a, const Candidate &b) {
		return a.offset > b.offset;
	});
	auto changed = false;
	for (const auto &candidate : _candidates) {
		const auto user = _resolved.value(candidate.username);
		if (!user) {
			continue;
		}
		const auto name = NicknameMentionTitle(user);
		if (name.isEmpty()) {
			continue;
		}
		const auto end = candidate.offset + candidate.length;
		const auto suffix = end == _snapshot.text.size() && _textPosition == end;
		const auto replacement = name + (suffix ? u" "_q : QString());
		const auto delta = int(replacement.size()) - candidate.length;
		const auto limit = _field->maxLength();
		if (updated.text.size() + delta > kMaxDraftLength
			|| (limit > 0 && updated.text.size() + delta > limit)) {
			continue;
		}
		const auto map = [&](int value, bool boundaryEnd) {
			return value >= end ? value + delta
				: value <= candidate.offset ? value
				: candidate.offset + (boundaryEnd ? int(replacement.size()) : 0);
		};
		for (auto &tag : updated.tags) {
			const auto till = map(tag.offset + tag.length, true);
			tag.offset = map(tag.offset, false);
			tag.length = till - tag.offset;
		}
		caret = map(caret, false);
		updated.text.replace(candidate.offset, candidate.length, replacement);
		updated.tags.push_back({ candidate.offset, int(name.size()), PrepareMentionTag(user) });
		changed = true;
	}
	if (!changed) {
		return;
	}
	updated.tags = TextUtilities::SimplifyTags(std::move(updated.tags));
	_applying = true;
	_field->setTextWithTags(updated);
	auto cursor = _field->textCursor();
	auto low = 0;
	auto high = _field->rawTextEdit()->document()->characterCount() - 1;
	while (low < high) {
		const auto middle = low + (high - low) / 2;
		if (_field->getTextWithTagsPart(0, middle).text.size() < caret) {
			low = middle + 1;
		} else {
			high = middle;
		}
	}
	cursor.setPosition(low);
	_field->setTextCursor(cursor);
	_applying = false;
}

} // namespace

QString NicknameMentionTitle(not_null<UserData*> user) {
	auto result = user->firstName;
	if (!user->lastName.isEmpty()) {
		if (!result.isEmpty()) {
			result += u' ';
		}
		result += user->lastName;
	}
	return result.isEmpty() ? user->name() : result;
}

void SetupNicknameMentions(
		not_null<Ui::InputField*> field,
		not_null<Main::Session*> session) {
	if (!field->property("regramNicknameMentionsAttached").toBool()) {
		field->setProperty("regramNicknameMentionsAttached", true);
		new NicknameMentions(field, session);
	}
}

} // namespace Nagram::Compose
