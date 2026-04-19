#pragma once
#include <string>
#include <functional>
#include "UIElement.hpp"
#include "ImGuiFileBrowser.hpp" 

class UiManager;

class FileSelectionWindow : public UIElement {
public:
    // RIMUOSSO IL BOOL: Da std::function<void(const std::string&, bool)>
    using LoadCallback = std::function<void(const std::string&)>; 
    
    explicit FileSelectionWindow(UiManager* manager, LoadCallback onLoadCallback);
    void draw() override;

private:
    UiManager* m_uiManager; 
    LoadCallback m_onLoadCallback;
    
    char m_filePathBuffer[260];
    
    // RIMOSSA LA RIGA: bool m_shouldGenerateCsv;

    ImGuiFileBrowser m_fileBrowser;
    bool m_showFileBrowser = false;
};