/**
 * @file IAssetManager.hpp
 * @brief OS-specific interface to load fonts, images and the default ImGui layout into memory.
 *
 * @details
 * Resources are shipped differently on every system:
 *  - **Windows**: embedded in the executable as RCDATA resources
 *    (assets/windows_resources.rc + assets/resources.h) -> AssetManager_Windows;
 *  - **macOS**: copied into the .app bundle under Contents/Resources (CMakeLists.txt) -> AssetManager_MacOS;
 *  - **Linux**: `assets/` folder copied next to the executable -> AssetManager_Linux.
 *
 * The right implementation is picked at compile time by CreateAssetManager().
 *
 * To add a NEW image or font:
 *  1. put the file in assets/images or assets/fonts;
 *  2. Windows: add an ID in assets/resources.h, the RCDATA line in
 *     assets/windows_resources.rc and the `if` branch in AssetManager_Windows::getImage()/getFont();
 *  3. macOS: add the file to the IMAGES_SOURCES / FONT_SOURCES lists in CMakeLists.txt;
 *  4. Linux: nothing to do (the whole assets folder is copied).
 *
 * The returned pointers stay valid as long as the asset manager (which owns
 * them) is alive: the caller must not free them.
 */
#pragma once
#include <string>
#include <filesystem>
#include <utility>
#include <cstdint>
#include <unordered_map>

/**
 * @brief Provides the binary content of the application resources.
 */
class IAssetManager {
public:
    virtual ~IAssetManager() = default;
    /**
     * @brief Loads a TrueType font.
     * @param name File name (e.g. "RobotoCondensed-Regular.ttf").
     * @return (data pointer, size in bytes) pair; {nullptr, 0} if not found.
     */
    virtual std::pair<void*, uint32_t> getFont(const std::string& name) = 0;
    /**
     * @brief Loads an image (PNG), to be decoded afterwards with stb_image.
     * @param path File name (e.g. "steering_wheel.png").
     * @return (data pointer, size in bytes) pair; {nullptr, 0} if not found.
     */
    virtual std::pair<void*, uint32_t> getImage(const std::string& path) = 0;
    /**
     * @brief Loads the default window layout (assets/resources/imgui.ini).
     * @return (text pointer, size in bytes) pair; {nullptr, 0} if not found.
     */
    virtual std::pair<void*, uint32_t> getDefaultImGuiIniFile() = 0;

private:
    /// Not used (the implementations have their own cache).
    std::unordered_map<std::string, std::pair<void*, uint32_t>> fontCache;
};
