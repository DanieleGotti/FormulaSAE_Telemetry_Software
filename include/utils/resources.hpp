#include <string>
#include <iostream>
#include <vector>
#include <utility>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h> // Per CFBundle
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace resources {
    std::string getResourcesPath();
    std::string getResourcePath(const std::string& relativePath);

#ifdef WIN32
    void loadEmbeddedFont(const wchar_t* resourceName);
    std::pair<void*, DWORD> GetFontData(LPWSTR resourceName);
#endif
} 