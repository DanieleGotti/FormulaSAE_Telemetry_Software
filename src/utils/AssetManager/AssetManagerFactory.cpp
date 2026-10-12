/**
 * @file AssetManagerFactory.cpp
 * @brief Picks at compile time the IAssetManager implementation for the current OS.
 *
 * @note The platform .cpp is included here directly (the classes are fully
 *       defined in their own files, with inline methods).
 */
#include <utils/AssetManagerFactory.hpp>

#ifdef _WIN32
#include "AssetManager_Windows.cpp"
#elif defined(__APPLE__)
#include "AssetManager_MacOS.cpp"
#elif defined(__linux__) || defined(__unix__)
#include "AssetManager_Linux.cpp"
#endif

std::shared_ptr<IAssetManager> CreateAssetManager() {
#ifdef _WIN32
    return std::make_shared<AssetManager_Windows>();
#elif defined(__APPLE__)
    return std::make_shared<AssetManager_MacOS>();
#elif defined(__linux__) || defined(__unix__)
  return std::make_shared<AssetManager_Linux>();
#else
  return nullptr; // unsupported platform: avoids undefined behavior
#endif
}
