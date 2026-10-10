#include "regram/nagram/core/relaunch.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>

namespace Nagram {

bool RelaunchWithoutUpdater(
		const QString &bundlePath,
		const QStringList &arguments) {
	const auto bundle = QDir(bundlePath);
	if (!bundle.dirName().endsWith(u".app"_q) || !bundle.exists()) {
		return false;
	} else if (QFile::exists(bundle.filePath(u"Contents/Frameworks/Updater"_q))) {
		return false;
	}
	// The new instance must start after this one releases the single-instance lock.
	const auto script = u"while /bin/kill -0 \"$1\" 2>/dev/null; do /bin/sleep 0.2; done; "
		"bundle=\"$2\"; shift 2; exec /usr/bin/open -n \"$bundle\" --args \"$@\""_q;
	auto full = QStringList{
		u"-c"_q,
		script,
		u"sh"_q,
		QString::number(QCoreApplication::applicationPid()),
		bundle.absolutePath(),
	};
	full.append(arguments);
	return QProcess::startDetached(u"/bin/sh"_q, full);
}

bool RunsFromPath(const QString &name) {
	const auto found = QStandardPaths::findExecutable(name);
	const auto self = QCoreApplication::applicationFilePath();
	return !found.isEmpty()
		&& (QFileInfo(found).canonicalFilePath()
			== QFileInfo(self).canonicalFilePath());
}

} // namespace Nagram
