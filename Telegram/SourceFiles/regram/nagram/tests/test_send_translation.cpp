#include "regram/nagram/services/send_translation_model.h"

#include <iostream>
#include <stdexcept>

namespace {

void Require(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

} // namespace

void TestSendTranslation() {
	using namespace Nagram;
	using namespace Nagram::SendTranslation;

	const auto one = WithLanguage({}, 42, u"en"_q);
	Require(one && Language(*one, 42) == u"en"_q
		&& Language(*one, 43).isEmpty()
		&& Language({}, 42).isEmpty(),
		"send translation language per chat");
	const auto two = WithLanguage(*one, 42, u"zh_CN"_q);
	Require(two && Language(*two, 42) == u"zh_CN"_q
		&& Parse(*two)->size() == 1,
		"send translation language replaced");
	const auto none = WithLanguage(*two, 42, QString());
	Require(none && none->isEmpty(),
		"an empty list is stored as nothing");
	Require(WithLanguage({}, 42, QString()) == QByteArray(),
		"removing a missing chat is not an error");
	Require(!WithLanguage({}, 0, u"en"_q)
		&& !WithLanguage({}, 42, u"../x"_q)
		&& !WithLanguage({}, 42, u"en us"_q)
		&& !WithLanguage({}, 42, QString(17, u'a'))
		&& !WithLanguage({}, 42, u"1en"_q)
		&& !WithLanguage("{broken", 42, u"en"_q),
		"send translation rejects bad input");

	auto full = Chats();
	for (auto i = 1; i <= kMaxChats; ++i) {
		full.emplace(i, u"en"_q);
	}
	const auto raw = Serialize(full);
	Require(Valid(raw)
		&& !WithLanguage(raw, kMaxChats + 1, u"en"_q)
		&& WithLanguage(raw, 1, u"de"_q)
		&& WithLanguage(raw, 1, QString()),
		"send translation chat limit");
	full.emplace(kMaxChats + 1, u"en"_q);
	Require(!Valid(Serialize(full)), "too many chats rejected");

	Require(Valid(R"({"version":1,"chats":{"5":"en"}})")
		&& !Valid(R"({"version":2,"chats":{"5":"en"}})")
		&& !Valid(R"({"version":1,"chats":{"05":"en"}})")
		&& !Valid(R"({"version":1,"chats":{"0":"en"}})")
		&& !Valid(R"({"version":1,"chats":{"x":"en"}})")
		&& !Valid(R"({"version":1,"chats":{"5":7}})")
		&& !Valid(R"({"version":1,"chats":{"5":""}})")
		&& !Valid(R"({"version":1,"chats":{"5":"en"},"more":1})")
		&& !Valid(R"({"version":1,"chats":[]})")
		&& !Valid("[]"),
		"send translation document validated");

	Require(Translatable(u"hello"_q)
		&& Translatable(u"  你好 "_q)
		&& Translatable(u"ok /start"_q)
		&& !Translatable(u"/start"_q)
		&& !Translatable(u"  /help me"_q)
		&& !Translatable(u"12:30 \U0001F44D"_q)
		&& !Translatable(u"   "_q)
		&& !Translatable(QString()),
		"commands and text without letters are sent as typed");

	auto registry = Registry();
	RegisterOptions(registry);
	Require(kEnabled.scope == Scope::Device
		&& kChats.scope == Scope::Account
		&& !registry.HasFlag(kChats.key, Flag::Exportable),
		"send translation option scopes");
	std::cout << "PASS: Nagram translation before sending" << std::endl;
}
