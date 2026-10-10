#pragma once

class QString;
class QVariant;

namespace Nagram::Links {

[[nodiscard]] bool HandleExternalLink(
	const QString &url,
	const QVariant &context);

} // namespace Nagram::Links
