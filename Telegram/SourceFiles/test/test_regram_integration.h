#pragma once

namespace Test {

void VerifyRegramIntegration();
void VerifyRegramBranding();
#ifdef Q_OS_MAC
void VerifyRegramMacWindow();
void VerifyRegramQtWindow();
void RecordRegramQtWindow(const QString &name);
#endif // Q_OS_MAC

} // namespace Test
