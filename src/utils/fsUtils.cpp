/**
 * @file fsUtils.cpp
 * @brief Implementation of the cross-platform system paths (see fsUtils.hpp).
 */
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif
#include <utils/fsUtils.hpp>

/// Name of the application data folder.
#define APPFOLDER "telemetry_manager"

namespace fsutils {
  std::filesystem::path getAppDocumentsFilePath(std::string relativePath) {
    return getAppDocumentsDirectory() / relativePath;
  }
  
  std::filesystem::path getAppDocumentsDirectory()  {
    // Computed only once, then reused
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
    if (configHome) {
        path = std::filesystem::path(configHome) / APPFOLDER;
    } else {
        const char* home = std::getenv("HOME");
        path = std::filesystem::path(home ? home : ".") / ".config" / APPFOLDER;
    }
#endif

    return path;
  }

  bool createAppDocumentsDirectory() {
    auto path = getAppDocumentsDirectory();
    std::filesystem::create_directories(path);
    return true;
  }

  std::filesystem::path getExecutableDirectory() {
        std::filesystem::path exeDir;
        
        // Executable path through the native API of each system
        #ifdef _WIN32
            char buffer[MAX_PATH];
            if (GetModuleFileNameA(NULL, buffer, MAX_PATH) != 0) {
                exeDir = std::filesystem::path(buffer).parent_path();
            }
        #elif defined(__APPLE__)
            char buffer[PATH_MAX];
            uint32_t size = sizeof(buffer);
            if (_NSGetExecutablePath(buffer, &size) == 0) {
                char real_path[PATH_MAX];
                if (realpath(buffer, real_path) != NULL) {
                    exeDir = std::filesystem::path(real_path).parent_path();
                } else {
                    exeDir = std::filesystem::path(buffer).parent_path();
                }
            }
        #elif defined(__linux__)
            char buffer[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
            if (count != -1) {
                exeDir = std::filesystem::path(std::string(buffer, count)).parent_path();
            }
        #endif

        // Fallback: current working directory
        if (exeDir.empty()) {
            exeDir = std::filesystem::current_path();
        }

        #if defined(__APPLE__)
        // On macOS, if we are inside a bundle (.app/Contents/MacOS), go 3 levels
        // up to place output_data exactly next to the .app file
        std::string exePathStr = exeDir.string();
        if (exePathStr.find(".app/Contents/MacOS") != std::string::npos) {
            exeDir = exeDir.parent_path().parent_path().parent_path();
        }
        #endif

        return exeDir;
    }
}