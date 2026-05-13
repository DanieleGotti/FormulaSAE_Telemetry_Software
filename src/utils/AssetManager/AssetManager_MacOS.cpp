#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <CoreFoundation/CoreFoundation.h>


#include <utils/IAssetManager.hpp>

class AssetManager_MacOS : public IAssetManager {
public:
    std::pair<void*, uint32_t> getFont(const std::string& name) override {
        return loadAsset("fonts/" + name);
    }

    std::pair<void*, uint32_t> getImage(const std::string& path) override {
        return loadAsset("images/" + path);
    }

    ~AssetManager_MacOS() {
        for (auto& [name, fontData] : m_assetCache) {
            if (fontData.first) {
                std::free(fontData.first);
            }
        }
    }

private:
    std::pair<void*, uint32_t> loadAsset(std::string path) {
        auto it = m_assetCache.find(path);
        if (it != m_assetCache.end())
            return it->second;

        auto filepath = std::filesystem::path(getResourcePath(path));
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {nullptr, 0};

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        void* data = malloc(size);
        file.read(reinterpret_cast<char*>(data), size);
        m_assetCache[path] = {data, static_cast<uint32_t>(size)};
        return m_assetCache[path];
    }

    std::string getResourcesPath() {
        std::cout << "DEBUG [AssetManager_MacOS]: Attempting to get the main bundle." << std::endl;
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        if (mainBundle) {
            std::cout << "DEBUG [AssetManager_MacOS]: Main bundle obtained." << std::endl;
            
            CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle); // Get the bundle's URL
            if (bundleURL) {
                std::cout << "DEBUG [AssetManager_MacOS]: Bundle URL obtained." << std::endl;
                CFStringRef bundlePathCFString = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle); // Get the absolute path of the bundle
                
                if (bundlePathCFString) {
                    std::cout << "DEBUG [AssetManager_MacOS]: Absolute path of the bundle obtained." << std::endl;
                    CFIndex length = CFStringGetLength(bundlePathCFString);
                    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                    
                    std::vector<char> buffer(maxSize + 1);
                    if (CFStringGetCString(bundlePathCFString, buffer.data(), buffer.size(), kCFStringEncodingUTF8)) {
                        std::string bundlePath = buffer.data();
                        std::cout << "DEBUG [AssetManager_MacOS]: Absolute path of the bundle: " << bundlePath << "." << std::endl;
                        
                        std::string resourcePath = bundlePath + "/Contents/Resources";
                        std::cout << "DEBUG [AssetManager_MacOS]: Resources path constructed: " << resourcePath << "." << std::endl;
                        
                        CFRelease(bundlePathCFString);
                        CFRelease(bundleURL);
                        return resourcePath;

                    } else {
                        std::cerr << "ERROR [AssetManager_MacOS]: CFStringGetCString (bundle path) failed." << std::endl;
                    }
                    CFRelease(bundlePathCFString);
                } else {
                    std::cerr << "ERROR [AssetManager_MacOS]: CFURLCopyFileSystemPath (bundle URL) returned NULL." << std::endl;
                }
                CFRelease(bundleURL);
            } else {
                std::cerr << "ERROR [AssetManager_MacOS]: CFBundleCopyBundleURL returned NULL." << std::endl;
            }
        } else {
            std::cerr << "ERROR [AssetManager_MacOS]: CFBundleGetMainBundle returned NULL." << std::endl;
        }
        std::cerr << "ERROR [AssetManager_MacOS]: Unable to find the resources directory of the bundle (getResourcesPath failed )." << std::endl;
        return "";
    }

    std::string getResourcePath(const std::string& relativePath) {
        std::string resourcesPath = getResourcesPath();
        if (!resourcesPath.empty()) {
            if (resourcesPath.back() != '/') {
                resourcesPath += "/";
            }
            return resourcesPath + relativePath;
        }

        return "";
    }

    std::pair<void*, uint32_t> getDefaultImGuiIniFile() override {
        std::string filename = "imgui.ini"; 

        std::cout << "INFO [AssetManager_MacOS]: Loading default ImGui config from bundle." << std::endl;

        return loadAsset(filename);
    }

private:
    std::unordered_map<std::string, std::pair<void*, uint32_t>> m_assetCache;
};
