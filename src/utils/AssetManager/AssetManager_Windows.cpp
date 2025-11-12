// AssetManager_Windows.cpp
#include <utils/IAssetManager.hpp>
#include <Windows.h> // se serve

class AssetManager_Windows : public IAssetManager {
public:
    void* getFont(const std::string& name) override {
        // usa WinAPI, resource pack, ecc.
    }
    void* getImage(const std::string& path) override {
        // carica asset da file system Windows
    }
};
