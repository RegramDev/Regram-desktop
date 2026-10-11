#include "regram/settings/ios_backup_model.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool passed, const char *message) {
	if (!passed) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestIosBackup() {
	using namespace Regram::Settings::IosBackup;
	const auto values = QJsonObject{
		{ u"quickTranslateButton"_q, true },
		{ u"accountColorsSaturation"_q, 73 },
		{ u"blockedPeerIds"_q, QJsonArray{ u"9007199254740993"_q } },
		{ u"messageFilterRules"_q, u"[]"_q },
		{ u"chatPreferences"_q, u"{}"_q },
		{ u"api_hash"_q, u"excluded"_q },
	};
	const auto bytes = Encode(values);
	const auto decoded = Parse(bytes);
	Require(!bytes.isEmpty() && decoded.error.isEmpty()
		&& decoded.values.size() == 5
		&& !decoded.values.contains(u"api_hash"_q)
		&& decoded.values.value(u"blockedPeerIds"_q).toArray().at(0)
			== QJsonValue(u"9007199254740993"_q),
		"iOS backup lost exact IDs or retained credentials");
	const auto translation = Select(decoded.values, { Category::Translation });
	Require(translation.size() == 1 && translation.value(u"quickTranslateButton"_q).toBool(),
		"selected category restored unrelated settings");
	auto bad = values;
	bad.insert(u"quickTranslateButton"_q, 1);
	Require(Encode(bad).isEmpty(), "integer accepted as a boolean");
	bad = values;
	bad.insert(u"accountColorsSaturation"_q, 101);
	Require(Encode(bad).isEmpty(), "out of range value accepted");
	bad = values;
	bad.insert(u"fontImportedLatin"_q, QString(64, u'a') + u":0"_q);
	Require(Encode(bad).isEmpty(), "noncanonical font face accepted");
	bad.insert(u"fontImportedLatin"_q, QString(64, u'a') + u":2"_q);
	Require(!Encode(bad).isEmpty(), "valid TTC face rejected");
	auto root = QJsonDocument::fromJson(bytes).object();
	root.insert(u"version"_q, 2);
	Require(!Parse(QJsonDocument(root).toJson()).error.isEmpty(), "future version accepted");
	root.insert(u"version"_q, 1);
	root.insert(u"createdAt"_q, u"invalid"_q);
	Require(!Parse(QJsonDocument(root).toJson()).error.isEmpty(), "invalid timestamp accepted");
	Require(!Parse(QByteArray(kMaximumBytes + 1, ' ')).error.isEmpty(), "oversize backup accepted");
	Require(CategoryFor(u"contextMenuOrder"_q) == Category::Messages
		&& CategoryFor(u"filterNotifications"_q) == Category::Notifications,
		"iOS category assignments changed");
	std::cout << "PASS: iOS settings backup format and category boundaries" << std::endl;
}
