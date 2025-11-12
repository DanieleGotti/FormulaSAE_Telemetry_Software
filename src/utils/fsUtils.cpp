#include <utils/fsUtils.hpp>

#define APPFOLDER "telemetry_manager"

namespace fsutils {
  std::filesystem::path getAppDocumentsFilePath(std::string relativePath) {
    return getAppDocumentsDirectory() / relativePath;
  }
  
  std::filesystem::path getAppDocumentsDirectory()  {
    static std::filesystem::path path;

    if(!path.empty()) return path;

#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    path = std::filesystem::path(home) / "Library/Application Support" / APPFOLDER;
    printf("Config path: %s\n", path.c_str());
#endif

#if defined (_WIN32) || defined( _WIN64)
    const char* appdata = std::getenv("APPDATA");
    path = std::filesystem::path(appdata) / APPFOLDER;
#endif

#if defined (__linux__)
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    if (!configHome) configHome = (std::string(std::getenv("HOME")) + "/.config").c_str();
    path = std::filesystem::path(configHome) / APPFOLDER;
#endif

    return path;
  }

  bool createAppDocumentsDirectory() {
    auto path = getAppDocumentsDirectory();
    std::filesystem::create_directories(path);
    return true;
  }
}