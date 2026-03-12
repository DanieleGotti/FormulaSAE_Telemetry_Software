#include <utils/IAssetManager.hpp>
#include <windows.h>
#include <string>
#include <utility>
#include <list>
#include <cstring>
#include <iostream>

#include "../../assets/resources.h"

class AssetManager_Windows : public IAssetManager {
public:
    ~AssetManager_Windows() override {
        for (void* buffer : m_assetDataBuffers) {
            delete[] static_cast<char*>(buffer);
        }
        m_assetDataBuffers.clear();
    }

    std::pair<void*, uint32_t> getFont(const std::string& name) override {
        int resourceId = 0;
        if (name == "RobotoCondensed-Regular.ttf") {
            resourceId = IDR_FONT_REGULAR;
        } else if (name == "RobotoCondensed-Bold.ttf") {
            resourceId = IDR_FONT_BOLD;
        } else {
            std::cerr << "ERRORE [AssetManager]: Font non trovato nella risorsa: '" << name << "'." <<std::endl;
            return {nullptr, 0};
        }

        std::cout << "INFO [AssetManager]: Caricamento font '" << name << "'." << std::endl;

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

    std::pair<void*, uint32_t> getImage(const std::string& path) override {
        int resourceId = 0;
        if (path == "steering_wheel.png") {
            resourceId = IDI_ICON_STEER;
        } else {
            std::cerr << "ERRORE [AssetManager]: Immagine non trovata nella risorsa: '" << path << "'." << std::endl;
            return {nullptr, 0};
        }

        std::cout << "INFO [AssetManager]: Caricamento immagine '" << path << "'." << std::endl;

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

        // Allocate resourceSize + 1 to ensure null-termination for safety
        char* safeBuffer = new char[resourceSize + 1];
        std::memcpy(safeBuffer, pResourceData, resourceSize);
        safeBuffer[resourceSize] = '\0';

        m_assetDataBuffers.push_back(safeBuffer); 
        return {safeBuffer, static_cast<uint32_t>(resourceSize)};
    }

private:
    std::list<void*> m_assetDataBuffers;
};