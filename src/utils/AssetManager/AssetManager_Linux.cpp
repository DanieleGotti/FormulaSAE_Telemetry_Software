#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <limits.h>

#include <utils/IAssetManager.hpp>

class AssetManager_Linux : public IAssetManager {
public:
    std::pair<void*, uint32_t> getFont(const std::string& name) override {
        return loadAsset("fonts/" + name);
    }

    std::pair<void*, uint32_t> getImage(const std::string& path) override {
        return loadAsset("images/" + path);
    }

    std::pair<void*, uint32_t> getDefaultImGuiIniFile() override {
        std::cout << "INFO [AssetManager_Linux]: Loading default ImGui config." << std::endl;
        // The default .ini file is located in the resources subfolder
        return loadAsset("resources/imgui.ini");
    }

    ~AssetManager_Linux() override {
        for (auto& [name, assetData] : m_assetCache) {
            if (assetData.first) {
                std::free(assetData.first);
            }
        }
    }

private:
    std::pair<void*, uint32_t> loadAsset(const std::string& relativePath) {
        // Check if the asset is already loaded in memory
        auto it = m_assetCache.find(relativePath);
        if (it != m_assetCache.end())
            return it->second;

        // Find the absolute path to the asset
        std::filesystem::path fullPath = findAssetPath(relativePath);
        if (fullPath.empty()) {
            std::cerr << "ERROR [AssetManager_Linux]: Asset not found: " << relativePath << std::endl;
            return {nullptr, 0};
        }

        // Open the file in binary mode and go to the end to get its size
        std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "ERROR [AssetManager_Linux]: Cannot open file: " << fullPath << std::endl;
            return {nullptr, 0};
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Allocate memory and read the file
        void* data = malloc(size);
        file.read(reinterpret_cast<char*>(data), size);
        m_assetCache[relativePath] = {data, static_cast<uint32_t>(size)};
        
        std::cout << "INFO [AssetManager_Linux]: Loaded asset " << relativePath << " from " << fullPath << "." << std::endl;
        return m_assetCache[relativePath];
    }

    std::filesystem::path findAssetPath(const std::string& relativePath) {
        // 1. Get the directory where the current executable is located
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        std::filesystem::path exeDir = std::filesystem::current_path();
        if (count != -1) {
            exeDir = std::filesystem::path(std::string(result, count)).parent_path();
        }

        // 2. Define possible search paths for the 'assets' directory
        std::vector<std::filesystem::path> searchPaths = {
            exeDir / "assets" / relativePath,                      // If running from release folder
            exeDir / ".." / "assets" / relativePath,               // If running from build/ folder
            std::filesystem::current_path() / "assets" / relativePath // Fallback to current working directory
        };

        // 3. Return the first valid path found
        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }

        return {};
    }

    std::unordered_map<std::string, std::pair<void*, uint32_t>> m_assetCache;
};