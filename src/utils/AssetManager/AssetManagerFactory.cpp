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
  return nullptr; // Fallback to prevent undefined behavior
#endif
}
