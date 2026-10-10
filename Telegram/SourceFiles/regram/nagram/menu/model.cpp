#include "regram/nagram/menu/model.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram::Menu {
namespace {

QByteArray Key(ActionId id) {
	return "E" + QByteArray::number(static_cast<int>(id)).rightJustified(2, '0');
}

bool KnownKey(const QString &key) {
	for (const auto &entry : kEntries) {
		if (key == QString::fromLatin1(Key(entry.id))) {
			return true;
		}
	}
	return false;
}

QJsonObject Parse(const QByteArray &raw) {
	return QJsonDocument::fromJson(raw).object();
}

} // namespace

bool IsUpstream(ActionId id) {
	return (static_cast<int>(id) < static_cast<int>(ActionId::Repeat))
		|| (id == ActionId::SeenBy);
}

Visibility DefaultVisibility(ActionId id) {
	return IsUpstream(id) ? Visibility::Show : Visibility::Hide;
}

bool ValidateConfig(const QByteArray &raw) {
	if (raw.isEmpty()) {
		return true;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(raw, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	const auto root = document.object();
	const auto version = root.value("version").toInt();
	if (root.size() != 2 || !root.value("version").isDouble()
		|| (version != 1 && version != 2)
		|| !root.value("states").isObject()) {
		return false;
	}
	const auto states = root.value("states").toObject();
	for (auto it = states.begin(); it != states.end(); ++it) {
		if ((!KnownKey(it.key()) && it.key() != u"E30")
			|| (version == 1 && it.key() == u"E22")
			|| !it.value().isString()) {
			return false;
		}
		const auto value = it.value().toString();
		if (value != u"show" && value != u"hide"
			&& value != u"option") {
			return false;
		}
	}
	return true;
}

Visibility ReadVisibility(const QByteArray &raw, ActionId id) {
	if (raw.isEmpty() || !ValidateConfig(raw)) {
		return DefaultVisibility(id);
	}
	const auto root = Parse(raw);
	if (root.value("version").toInt() == 1) {
		if (id == ActionId::Screenshot) {
			return DefaultVisibility(id);
		}
		if (id == ActionId::Reading) {
			id = ActionId::Screenshot;
		}
	}
	const auto value = root.value("states").toObject().value(
		QString::fromLatin1(Key(id))).toString();
	return (value == u"hide")
		? Visibility::Hide
		: (value == u"option")
		? Visibility::WithOption
		: value == u"show"
		? Visibility::Show
		: DefaultVisibility(id);
}

QByteArray WriteVisibility(
		const QByteArray &raw,
		ActionId id,
		Visibility visibility) {
	Expects(ValidateConfig(raw));
	auto states = raw.isEmpty()
		? QJsonObject()
		: Parse(raw).value("states").toObject();
	if (!raw.isEmpty() && Parse(raw).value("version").toInt() == 1) {
		const auto oldReading = states.take(u"E21");
		if (!oldReading.isUndefined()) {
			states.insert(u"E22", oldReading);
		}
	}
	states.remove(u"E30");
	const auto key = QString::fromLatin1(Key(id));
	if (visibility == DefaultVisibility(id)) {
		states.remove(key);
	} else {
		states.insert(key, visibility == Visibility::Hide
			? QString(u"hide")
			: visibility == Visibility::WithOption
			? QString(u"option") : QString(u"show"));
	}
	if (states.isEmpty()) {
		return QByteArray();
	}
	auto root = QJsonObject();
	root.insert(u"version", 2);
	root.insert(u"states", states);
	return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool Visible(Visibility visibility, bool optionHeld) {
	return visibility == Visibility::Show
		|| (visibility == Visibility::WithOption && optionHeld);
}

int EndPosition(const std::vector<Slot> &slots) {
	const auto count = int(slots.size());
	for (auto index = 0; index != count; ++index) {
		if (slots[index].id == ActionId::Delete) {
			return index;
		}
	}
	// WHY: upstream closes the menu with separated info rows (seen by,
	// edited or forwarded time, author), all untagged except SeenBy, so
	// the first separator below the last tagged action starts that footer.
	auto footer = count;
	for (auto index = count; index != 0;) {
		--index;
		const auto &slot = slots[index];
		if (slot.separator) {
			footer = index;
		} else if (slot.id && slot.id != ActionId::SeenBy) {
			return footer;
		}
	}
	return count;
}

} // namespace Nagram::Menu
