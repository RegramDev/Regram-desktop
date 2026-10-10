#ifdef _DEBUG

#include "test/test_regram_integration.h"

#include "test/test_agent.h"
#include "test/test_log.h"
#include "core/application.h"
#include "regram/nagram/services/credentials.h"
#include "mainwindow.h"
#include "window/window_controller.h"

#include <Cocoa/Cocoa.h>
#include <Security/Security.h>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtWidgets/QApplication>

namespace Test {

namespace {

void VerifyKeychainIdentity() {
	const auto account = [@"regram-branding-fixture-" stringByAppendingString:NSUUID.UUID.UUIDString];
	const auto secret = [@"fictional-regram-service-secret" dataUsingEncoding:NSUTF8StringEncoding];
	auto legacy = [@{
		(__bridge NSString*)kSecClass: (__bridge NSString*)kSecClassGenericPassword,
		(__bridge NSString*)kSecAttrService: @"Nagram Desktop Services",
		(__bridge NSString*)kSecAttrAccount: account,
		(__bridge NSString*)kSecUseAuthenticationUI: (__bridge NSString*)kSecUseAuthenticationUIFail,
	} mutableCopy];
	auto current = [legacy mutableCopy];
	current[(__bridge NSString*)kSecAttrService] = @"Regram Desktop Services";
	const auto cleanup = gsl::finally([&] {
		for (const auto query : { legacy, current }) {
			const auto status = SecItemDelete((__bridge CFDictionaryRef)query);
			Check(status == errSecSuccess || status == errSecItemNotFound,
				u"Fictional keychain fixture cleaned up"_q);
		}
		[legacy release];
		[current release];
	});
	auto insertion = [legacy mutableCopy];
	insertion[(__bridge NSString*)kSecValueData] = secret;
	const auto status = SecItemAdd((__bridge CFDictionaryRef)insertion, nullptr);
	[insertion release];
	Check(status == errSecSuccess, u"Fictional legacy keychain entry created"_q,
		u"status=%1"_q.arg(status));
	if (status != errSecSuccess) {
		return;
	}
	const auto result = Nagram::ReadCredential(QString::fromNSString(account));
	Check(result.error == Nagram::CredentialError::None
		&& result.secret == "fictional-regram-service-secret",
		u"Existing service credential survives the brand migration"_q);
	Check(SecItemCopyMatching((__bridge CFDictionaryRef)current, nullptr) == errSecSuccess,
		u"Service credential is stored under Regram identity"_q);
	Check(SecItemCopyMatching((__bridge CFDictionaryRef)legacy, nullptr) == errSecItemNotFound,
		u"Migrated legacy keychain label is removed"_q);
	Check(Nagram::DeleteCredential(QString::fromNSString(account)) == Nagram::CredentialError::None,
		u"Migrated service credential can be deleted"_q);
}

} // namespace

void VerifyRegramQtWindow() {
	const auto controller = Core::App().activePrimaryWindow();
	Check(controller && !controller->widget()->isHidden(),
		u"Original Qt window is shown"_q);
	if (!controller) return;
	const auto root = controller->widget();
	const auto view = reinterpret_cast<NSView*>(root->winId());
	Check([NSStringFromClass(view.class) containsString:@"QNSView"],
		u"Main content is owned by Qt"_q);
	Check(view.window && !view.window.toolbar,
		u"Qt window has no custom material toolbar"_q);
	for (NSWindow *window in NSApp.windows) {
		Check(![window.identifier isEqualToString:@"regram.native.window"],
			u"Removed native preview cannot open"_q);
	}
	const auto widgets = QApplication::allWidgets();
	Check(ranges::none_of(widgets, [](QWidget *widget) {
		return widget->property("regramGlassPanel").isValid();
	}), u"Qt widgets have no glass panel adapters"_q);
}

void RecordRegramQtWindow(const QString &name) {
	const auto controller = Core::App().activePrimaryWindow();
	if (!controller) return;
	const auto view = reinterpret_cast<NSView*>(controller->widget()->winId());
	auto file = QSaveFile(EvidenceDir() + name + u".json"_q);
	const auto bytes = QJsonDocument(QJsonObject{
		{ u"window"_q, int(view.window.windowNumber) },
		{ u"interface"_q, u"Qt"_q },
		{ u"qt"_q, QString::fromLatin1(qVersion()) },
	}).toJson();
	Check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(),
		u"Qt window capture metadata saved"_q);
}

void VerifyRegramMacWindow() {
	if (!Active()) {
		return;
	}
	VerifyKeychainIdentity();
	const auto controller = Core::App().activePrimaryWindow();
	Check(controller != nullptr, u"Regram primary window exists"_q);
	if (!controller) {
		return;
	}
	const auto view = reinterpret_cast<NSView*>(controller->widget()->winId());
	const auto window = view.window;
	Check(window && window.isVisible, u"Native macOS window is visible"_q);
	if (!window) {
		return;
	}
	Check(!window.toolbar, u"Regram uses its standard native window"_q);
	const auto frame = window.contentView.superview;
	const auto bitmap = [frame bitmapImageRepForCachingDisplayInRect:frame.bounds];
	if (bitmap) {
		[frame cacheDisplayInRect:frame.bounds toBitmapImageRep:bitmap];
		const auto data = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
		const auto path = ScreenshotsDir() + u"regram-native-window.png"_q;
		const auto saved = [data writeToFile:path.toNSString() atomically:YES];
		Check(saved, u"Native window frame captured"_q,
			u"pixels=%1x%2"_q.arg(bitmap.pixelsWide).arg(bitmap.pixelsHigh));
	} else {
		Fail(u"Native window bitmap could not be allocated"_q);
	}
}

} // namespace Test

#endif // _DEBUG
