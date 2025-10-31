#include <utils/resources.hpp>

namespace resources {

std::string getResourcesPath() {
#ifdef __APPLE__
    std::cout << "DEBUG getResourcesPath: Tentativo di ottenere il main bundle." << std::endl;
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
        std::cout << "DEBUG getResourcesPath: Main bundle ottenuto." << std::endl;
        
        CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle); // Ottieni l'URL del bundle
        if (bundleURL) {
            std::cout << "DEBUG getResourcesPath: URL del bundle ottenuto." << std::endl;
            CFStringRef bundlePathCFString = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle); // Ottieni il percorso assoluto del bundle
            
            if (bundlePathCFString) {
                std::cout << "DEBUG getResourcesPath: Percorso assoluto del bundle ottenuto." << std::endl;
                CFIndex length = CFStringGetLength(bundlePathCFString);
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                
                std::vector<char> buffer(maxSize + 1);
                if (CFStringGetCString(bundlePathCFString, buffer.data(), buffer.size(), kCFStringEncodingUTF8)) {
                    std::string bundlePath = buffer.data();
                    std::cout << "DEBUG getResourcesPath: Percorso base bundle: " << bundlePath << std::endl;
                    
                    std::string resourcePath = bundlePath + "/Contents/Resources";
                    std::cout << "DEBUG getResourcesPath: Percorso Resources costruito: " << resourcePath << std::endl;
                    
                    CFRelease(bundlePathCFString);
                    CFRelease(bundleURL);
                    return resourcePath;

                } else {
                    std::cerr << "ERRORE getResourcesPath: CFStringGetCString (bundle path) fallito." << std::endl;
                }
                CFRelease(bundlePathCFString);
            } else {
                std::cerr << "ERRORE getResourcesPath: CFURLCopyFileSystemPath (bundle URL) ha restituito NULL." << std::endl;
            }
            CFRelease(bundleURL);
        } else {
            std::cerr << "ERRORE getResourcesPath: CFBundleCopyBundleURL ha restituito NULL." << std::endl;
        }
    } else {
        std::cerr << "ERRORE getResourcesPath: CFBundleGetMainBundle ha restituito NULL." << std::endl;
    }
    std::cerr << "ERRORE CRITICO: Impossibile trovare la directory Resources del bundle (getResourcesPath fallito)." << std::endl;
    return "";
#else
    return ""; 
#endif
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

#ifdef __APPLE__
    return ""; 
#else
    return "../external/" + relativePath; 
#endif
}

#ifdef _WIN32
void loadEmbeddedFont(const wchar_t* resourceName) {
    // Trova la risorsa
    HRSRC hRes = FindResourceW(nullptr, resourceName, RT_RCDATA);
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

std::pair<void*, DWORD> GetFontData(const wchar_t* resourceName) {
    HRSRC hRes = FindResourceW(nullptr, resourceName, RT_RCDATA);
    if (!hRes) return {nullptr, 0};
    HGLOBAL hMem = LoadResource(nullptr, hRes);
    DWORD size = SizeofResource(nullptr, hRes);
    void* pData = LockResource(hMem);
    return {pData, size};
}
#endif

}