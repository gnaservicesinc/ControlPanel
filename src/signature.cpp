#include "appexport.h"
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#endif

namespace cp {
bool verifyAppSignature(const QString &bundle, QString *error) {
#ifdef Q_OS_MACOS
    const auto path = bundle.toUtf8();
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8 *>(path.constData()), path.size(), true);
    SecStaticCodeRef code = nullptr;
    OSStatus status = url ? SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code) : errSecParam;
    if (url) CFRelease(url);
    if (status == errSecSuccess) {
        status = SecStaticCodeCheckValidity(code, kSecCSStrictValidate | kSecCSCheckAllArchitectures | kSecCSCheckNestedCode, nullptr);
        CFRelease(code);
    }
    if (status == errSecSuccess) return true;
    *error = QString("The app’s signature could not be verified (error %1). It may have been changed or damaged. Ask your support contact for a fresh copy.").arg(status);
#else
    Q_UNUSED(bundle);
    *error = "App export requires macOS.";
#endif
    return false;
}
}
