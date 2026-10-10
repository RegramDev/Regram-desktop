#include "regram/nagram/services/credentials.h"

#ifdef Q_OS_MAC
#include <Security/Security.h>
#elif defined Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace Nagram {
namespace {

bool ValidAccount(const QString &account) {
	return !account.isEmpty()
		&& account.size() <= 256
		&& !account.contains(QChar(0));
}

#ifdef Q_OS_MAC

class Query {
public:
	explicit Query(const QString &account, bool legacy = false)
	: _value(CFDictionaryCreateMutable(
		nullptr,
		0,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks)) {
		const auto bytes = account.toUtf8();
		const auto key = CFStringCreateWithBytes(
			nullptr,
			reinterpret_cast<const UInt8*>(bytes.constData()),
			bytes.size(),
			kCFStringEncodingUTF8,
			false);
		CFDictionarySetValue(_value, kSecClass, kSecClassGenericPassword);
		CFDictionarySetValue(_value, kSecAttrService, legacy
			? CFSTR("Nagram Desktop Services")
			: CFSTR("Regram Desktop Services"));
		CFDictionarySetValue(_value, kSecAttrAccount, key);
		CFDictionarySetValue(_value, kSecAttrSynchronizable, kCFBooleanFalse);
		CFRelease(key);
	}

	~Query() {
		CFRelease(_value);
	}

	CFMutableDictionaryRef get() const {
		return _value;
	}

private:
	CFMutableDictionaryRef _value = nullptr;

};

CredentialError Error(OSStatus status) {
	return (status == errSecSuccess) ? CredentialError::None
		: (status == errSecItemNotFound) ? CredentialError::Missing
		: CredentialError::Denied;
}

#endif

CredentialResult ReadCredentialFrom(const QString &account, bool legacy) {
	if (!ValidAccount(account)) {
		return { .error = CredentialError::Invalid };
	}
#ifdef Q_OS_MAC
	auto query = Query(account, legacy);
	CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);
	CFDictionarySetValue(query.get(), kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
	auto result = CFTypeRef(nullptr);
	const auto status = SecItemCopyMatching(query.get(), &result);
	if (status != errSecSuccess) {
		return { .error = Error(status) };
	}
	if (!result || CFGetTypeID(result) != CFDataGetTypeID()) {
		if (result) {
			CFRelease(result);
		}
		return { .error = CredentialError::Invalid };
	}
	const auto data = static_cast<CFDataRef>(result);
	auto secret = QByteArray(
		reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
		CFDataGetLength(data));
	CFRelease(result);
	return { .secret = std::move(secret) };
#elif defined Q_OS_WIN
	const auto target = (legacy
		? u"Nagram Desktop Services/"_q
		: u"Regram Desktop Services/"_q) + account;
	auto credential = PCREDENTIALW(nullptr);
	if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
		return { .error = GetLastError() == ERROR_NOT_FOUND
			? CredentialError::Missing : CredentialError::Denied };
	}
	auto secret = QByteArray(
		reinterpret_cast<const char*>(credential->CredentialBlob),
		credential->CredentialBlobSize);
	CredFree(credential);
	return { .secret = std::move(secret) };
#else
	return { .error = CredentialError::Unavailable };
#endif
}

} // namespace

CredentialError WriteCredential(const QString &account, const QByteArray &secret) {
	if (!ValidAccount(account) || secret.isEmpty() || secret.size() > 4096
		|| secret.contains('\0') || secret.contains('\r') || secret.contains('\n')) {
		return CredentialError::Invalid;
	}
#ifdef Q_OS_MAC
	auto query = Query(account);
	const auto data = CFDataCreate(
		nullptr,
		reinterpret_cast<const UInt8*>(secret.constData()),
		secret.size());
	const auto changes = CFDictionaryCreateMutable(
		nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(changes, kSecValueData, data);
	auto status = SecItemUpdate(query.get(), changes);
	if (status == errSecItemNotFound) {
		CFDictionarySetValue(query.get(), kSecValueData, data);
		status = SecItemAdd(query.get(), nullptr);
	}
	CFRelease(changes);
	CFRelease(data);
	return Error(status);
#elif defined Q_OS_WIN
	const auto target = u"Regram Desktop Services/"_q + account;
	auto credential = CREDENTIALW{};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
	credential.CredentialBlobSize = secret.size();
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.constData()));
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	return CredWriteW(&credential, 0) ? CredentialError::None : CredentialError::Denied;
#else
	return CredentialError::Unavailable;
#endif
}

namespace {

CredentialError DeleteCredentialFrom(const QString &account, bool legacy) {
	if (!ValidAccount(account)) {
		return CredentialError::Invalid;
	}
#ifdef Q_OS_MAC
	auto query = Query(account, legacy);
	const auto status = SecItemDelete(query.get());
	return status == errSecItemNotFound ? CredentialError::None : Error(status);
#elif defined Q_OS_WIN
	const auto target = (legacy
		? u"Nagram Desktop Services/"_q
		: u"Regram Desktop Services/"_q) + account;
	return (CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0)
		|| GetLastError() == ERROR_NOT_FOUND)
		? CredentialError::None : CredentialError::Denied;
#else
	return CredentialError::Unavailable;
#endif
}

} // namespace

CredentialResult ReadCredential(const QString &account) {
	auto result = ReadCredentialFrom(account, false);
	if (result.error != CredentialError::Missing) {
		return result;
	}
	result = ReadCredentialFrom(account, true);
	if (result.error == CredentialError::None) {
		const auto migrated = WriteCredential(account, result.secret);
		if (migrated == CredentialError::None) {
			const auto removed = DeleteCredentialFrom(account, true);
			if (removed != CredentialError::None) {
				LOG(("Regram: legacy service credential cleanup failed."));
			}
		}
	}
	return result;
}

CredentialError DeleteCredential(const QString &account) {
	const auto current = DeleteCredentialFrom(account, false);
	return (current == CredentialError::None)
		? DeleteCredentialFrom(account, true)
		: current;
}

} // namespace Nagram
