/**
 * @file AssetManager_Linux.cpp
 * @brief Linux asset manager: reads the files from the `assets/` folder next to the executable.
 *
 * @details
 * CMakeLists.txt copies the assets folder into the build folder. Files are
 * searched, in order, in: `<exe>/assets`, `<exe>/../assets`, `<cwd>/assets`.
 * Every file read stays cached (memory allocated with malloc and freed in the destructor).
 */
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

/**
 * @brief File system based IAssetManager implementation (Linux).
 */
class AssetManager_Linux : public IAssetManager {
public:
    /** @brief Loads `assets/fonts/<name>`. */
    std::pair<void*, uint32_t> getFont(const std::string& name) override {
        return loadAsset("fonts/" + name);
    }

    /** @brief Loads `assets/images/<path>`. */
    std::pair<void*, uint32_t> getImage(const std::string& path) override {
        return loadAsset("images/" + path);
    }

    /** @brief Loads `assets/resources/imgui.ini`. */
    std::pair<void*, uint32_t> getDefaultImGuiIniFile() override {
        std::cout << "INFO [AssetManager_Linux]: Loading default ImGui config." << std::endl;
        // The default .ini file is located in the resources subfolder
        return loadAsset("resources/imgui.ini");
    }

    /** @brief Frees every cached buffer. */
    ~AssetManager_Linux() override {
        for (auto& [name, assetData] : m_assetCache) {
            if (assetData.first) {
                std::free(assetData.first);
            }
        }
    }

private:
    /**
     * @brief Reads an asset file into memory (cached).
     * @param relativePath Path relative to the assets folder.
     * @return (data, size) or {nullptr, 0} if the file does not exist.
     */
    std::pair<void*, uint32_t> loadAsset(const std::string& relativePath) {
        // Already loaded: return the copy in memory
        auto it = m_assetCache.find(relativePath);
        if (it != m_assetCache.end())
            return it->second;

        // Find the absolute path of the asset
        std::filesystem::path fullPath = findAssetPath(relativePath);
        if (fullPath.empty()) {
            std::cerr << "ERROR [AssetManager_Linux]: Asset not found: " << relativePath << std::endl;
            return {nullptr, 0};
        }

        // Open in binary mode, positioned at the end to get the size
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

    /**
     * @brief Looks for the file in the candidate locations of the assets folder.
     * @param relativePath Path relative to the assets folder.
     * @return Existing path, or an empty path if not found.
     */
    std::filesystem::path findAssetPath(const std::string& relativePath) {
        // 1. Folder containing the current executable
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        std::filesystem::path exeDir = std::filesystem::current_path();
        if (count != -1) {
            exeDir = std::filesystem::path(std::string(result, count)).parent_path();
        }

        // 2. Possible locations of the 'assets' folder
        std::vector<std::filesystem::path> searchPaths = {
            exeDir / "assets" / relativePath,                      // running from the release folder
            exeDir / ".." / "assets" / relativePath,               // running from the build/ folder
            std::filesystem::current_path() / "assets" / relativePath // fallback: current working directory
        };

        // 3. Return the first existing path
        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }

        return {};
    }

    /// Cache: relative path -> (data, size).
    std::unordered_map<std::string, std::pair<void*, uint32_t>> m_assetCache;
};