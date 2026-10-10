#include "regram/typography/font_data.h"

#ifdef Q_OS_WIN
#include <QtCore/QScopeGuard>
#include <QtCore/QtEndian>
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#endif // Q_OS_WIN

namespace Regram::Typography {

QByteArray NativeFontData(const QByteArray &bytes) {
#ifdef Q_OS_WIN
	if (bytes.startsWith("wOF2") || bytes.startsWith("wOFF")) {
		constexpr auto kMaximumBytes = 64 * 1024 * 1024;
		if (bytes.size() < 48 || bytes.size() > kMaximumBytes
			|| qFromBigEndian<quint32>(bytes.constData() + 16) > kMaximumBytes) {
			return {};
		}
		auto factory = Microsoft::WRL::ComPtr<IDWriteFactory5>();
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,
			__uuidof(IDWriteFactory5),
			reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
			return {};
		}
		auto stream = Microsoft::WRL::ComPtr<IDWriteFontFileStream>();
		if (FAILED(factory->UnpackFontFile(
			bytes.startsWith("wOF2") ? DWRITE_CONTAINER_TYPE_WOFF2 : DWRITE_CONTAINER_TYPE_WOFF,
			bytes.constData(), UINT32(bytes.size()), stream.GetAddressOf()))) {
			return {};
		}
		auto size = UINT64(0);
		if (FAILED(stream->GetFileSize(&size)) || !size || size > kMaximumBytes) {
			return {};
		}
		auto fragment = static_cast<const void*>(nullptr);
		auto context = static_cast<void*>(nullptr);
		if (FAILED(stream->ReadFileFragment(&fragment, 0, size, &context))) {
			return {};
		}
		const auto guard = qScopeGuard([&] { stream->ReleaseFileFragment(context); });
		return QByteArray(static_cast<const char*>(fragment), qsizetype(size));
	}
#endif // Q_OS_WIN
	return bytes;
}

} // namespace Regram::Typography
