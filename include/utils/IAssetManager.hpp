#pragma once
#include <string>
#include <filesystem>
#include <utility>
#include <cstdint>
#include <unordered_map>

class IAssetManager {
public:
    virtual ~IAssetManager() = default;
    virtual std::pair<void*, uint32_t> getFont(const std::string& name) = 0;
    virtual std::pair<void*, uint32_t> getImage(const std::string& path) = 0;
    virtual std::pair<void*, uint32_t> getDefaultImGuiIniFile() = 0;

private:
    std::unordered_map<std::string, std::pair<void*, uint32_t>> fontCache;
};
