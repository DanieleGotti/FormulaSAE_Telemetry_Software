#include <utils/AssetManagerFactory.hpp>

#ifdef _WIN32
#include "AssetManager_Windows.cpp"
#elif defined(__APPLE__)
#include "AssetManager_MacOS.cpp"
#endif

std::shared_ptr<IAssetManager> CreateAssetManager() {
#ifdef _WIN32
    return std::make_shared<AssetManager_Windows>();
#elif defined(__APPLE__)
    return std::make_shared<AssetManager_MacOS>();
#endif
}
