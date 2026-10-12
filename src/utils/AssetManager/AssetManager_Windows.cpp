/**
 * @file AssetManager_Windows.cpp
 * @brief Windows asset manager: reads fonts and images embedded in the executable as RCDATA resources.
 *
 * @details
 * Resources are declared in assets/windows_resources.rc with the IDs of
 * assets/resources.h and compiled into the .exe (no external files to ship).
 * Every requested file name must be mapped to its ID in the `if` branches
 * below: for a new image/font add the ID, the .rc line and the `if` branch.
 * Data is copied into private buffers (freed in the destructor).
 */
#include <windows.h>
#include <string>
#include <utility>
#include <list>
#include <cstring>
#include <iostream>
#include <utils/IAssetManager.hpp>
#include "../../assets/resources.h"

/**
 * @brief IAssetManager implementation based on the embedded Win32 resources.
 */
class AssetManager_Windows : public IAssetManager {
public:
    /** @brief Frees every resource copy handed out. */
    ~AssetManager_Windows() override {
        for (void* buffer : m_assetDataBuffers) {
            delete[] static_cast<char*>(buffer);
        }
        m_assetDataBuffers.clear();
    }

    /**
     * @brief Returns an embedded font.
     * @param name "RobotoCondensed-Regular.ttf" or "RobotoCondensed-Bold.ttf".
     * @return (data, size) or {nullptr, 0} if the name is not mapped.
     */
    std::pair<void*, uint32_t> getFont(const std::string& name) override {
        // File name -> resource ID (see assets/resources.h)
        int resourceId = 0;
        if (name == "RobotoCondensed-Regular.ttf") {
            resourceId = IDR_FONT_REGULAR;
        } else if (name == "RobotoCondensed-Bold.ttf") {
            resourceId = IDR_FONT_BOLD;
        } else {
            std::cerr << "ERROR [AssetManager]: Font not found in resource: '" << name << "'." <<std::endl;
            return {nullptr, 0};
        }

        std::cout << "INFO [AssetManager]: Loading font '" << name << "'." << std::endl;

        // Locate and lock the resource inside the executable
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hRes) return {nullptr, 0};
        
        HGLOBAL hResLoad = LoadResource(hInstance, hRes);
        void* pResourceData = LockResource(hResLoad);
        DWORD resourceSize = SizeofResource(hInstance, hRes);
        if (!pResourceData || resourceSize == 0) return {nullptr, 0};
        
        // Copy into a private buffer (the resource memory is read-only)
        char* safeBuffer = new char[resourceSize];
        std::memcpy(safeBuffer, pResourceData, resourceSize);
        
        m_assetDataBuffers.push_back(safeBuffer); 
        return {safeBuffer, static_cast<uint32_t>(resourceSize)};
    }

    /**
     * @brief Returns an embedded image.
     * @param path "steering_wheel.png", "logo_erb.png" or "icon.png".
     * @return (data, size) or {nullptr, 0} if the name is not mapped.
     */
    std::pair<void*, uint32_t> getImage(const std::string& path) override {
        // File name -> resource ID (see assets/resources.h)
        int resourceId = 0;
        if (path == "steering_wheel.png") {
            resourceId = IDI_ICON_STEER;
        } else if (path == "logo_erb.png") {
            resourceId = IDI_ICON_ERB; 
        } else if (path == "icon.png") {
            resourceId = IDI_ICON_LOGO;
        } else {
            std::cerr << "ERROR [AssetManager]: Image not found in resource: '" << path << "'." << std::endl;
            return {nullptr, 0};
        }

        std::cout << "INFO [AssetManager]: Loading image '" << path << "'." << std::endl;

        HINSTANCE hInstance = GetModuleHandle(nullptr);
        HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hRes) return {nullptr, 0};

        HGLOBAL hResLoad = LoadResource(hInstance, hRes);
        void* pResourceData = LockResource(hResLoad);
        DWORD resourceSize = SizeofResource(hInstance, hRes);
        if (!pResourceData || resourceSize == 0) return {nullptr, 0};

        char* safeBuffer = new char[resourceSize];
        std::memcpy(safeBuffer, pResourceData, resourceSize);

        m_assetDataBuffers.push_back(safeBuffer); 
        return {safeBuffer, static_cast<uint32_t>(resourceSize)};
    }

    /**
     * @brief Returns the default window layout (IDR_IMGUI_CONFIG resource).
     * @return ('\0' terminated text, size without terminator) or {nullptr, 0}.
     */
    std::pair<void*, uint32_t> getDefaultImGuiIniFile() {
        int resourceId = IDR_IMGUI_CONFIG;

        std::cout << "INFO [AssetManager]: Loading ImGui config resource" << std::endl;

        HINSTANCE hInstance = GetModuleHandle(nullptr);
        HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hRes) return {nullptr, 0};

        HGLOBAL hResLoad = LoadResource(hInstance, hRes);
        void* pResourceData = LockResource(hResLoad);
        DWORD resourceSize = SizeofResource(hInstance, hRes);
        
        if (!pResourceData || resourceSize == 0) return {nullptr, 0};

        // Allocate resourceSize + 1 to guarantee the '\0' terminator (it is text)
        char* safeBuffer = new char[resourceSize + 1];
        std::memcpy(safeBuffer, pResourceData, resourceSize);
        safeBuffer[resourceSize] = '\0';

        m_assetDataBuffers.push_back(safeBuffer); 
        return {safeBuffer, static_cast<uint32_t>(resourceSize)};
    }

private:
    /// Buffers allocated with new[] and handed to the caller, freed in the destructor.
    std::list<void*> m_assetDataBuffers;
};