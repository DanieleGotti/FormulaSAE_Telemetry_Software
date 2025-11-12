#include <utils/resources.hpp>

#ifdef WIN32
#include "../assets/resources.h"
#endif

namespace resources {

std::string getResourcesPath() {
    return "";
}

// Funzione helper per combinare il percorso della risorsa
std::string getResourcePath(const std::string& relativePath) {
    std::string resourcesPath = getResourcesPath();
    if (!resourcesPath.empty()) {
        if (resourcesPath.back() != '/') {
            resourcesPath += "/";
        }
        return resourcesPath + relativePath;
    }
    return "../external/" + relativePath; 
}

#ifdef WIN32
void loadEmbeddedFont(const wchar_t* resourceName) {
    // Trova la risorsa
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(IDR_FONT_REGULAR), TEXT("RCDATA"));
    if (!hRes) {
        std::wcerr << L"Font resource not found: " << resourceName << std::endl;
        return;
    }

    // Carica la risorsa in memoria
    HGLOBAL hMem = LoadResource(nullptr, hRes);
    DWORD size = SizeofResource(nullptr, hRes);
    void* pData = LockResource(hMem);

    if (!pData || size == 0) {
        std::wcerr << L"Error loading font resource: " << resourceName << std::endl;
        return;
    }

    // Copia in buffer e registra il font in memoria
    DWORD fontsAdded = 0;
    HANDLE fontHandle = AddFontMemResourceEx(pData, size, nullptr, &fontsAdded);

    if (fontHandle == nullptr || fontsAdded == 0) {
        std::wcerr << L"AddFontMemResourceEx failed for: " << resourceName << std::endl;
    } else {
        std::wcout << L"Loaded embedded font: " << resourceName << std::endl;
    }
}

std::pair<void*, DWORD> GetFontData(LPWSTR resourceName) {
    HRSRC hRes = FindResourceW(nullptr, resourceName, RT_RCDATA);
    if (!hRes) return {nullptr, 0};
    HGLOBAL hMem = LoadResource(nullptr, hRes);
    if (!hMem) return {nullptr, 0}; // Aggiunge controllo per LoadResource
    DWORD size = SizeofResource(nullptr, hRes);
    void* pData = LockResource(hMem);
    // LockResource restituisce NULL se hMem non è valido o se la risorsa è vuota.
    // Non c'è bisogno di sbloccare, il sistema operativo gestisce la memoria della risorsa.
    return {pData, size};
}
#endif

}