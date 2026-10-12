/**
 * @file FileSelectionWindow.hpp
 * @brief "Load from file" window: choice of a .bin/.csv file to replay.
 */
#pragma once
#include <string>
#include <functional>
#include "UIElement.hpp"
#include "ImGuiFileBrowser.hpp" 

class UiManager;

/**
 * @brief Home screen window used to open a recording.
 *
 * The path can be typed by hand or chosen with the "Browse" button
 * (ImGuiFileBrowser). "Load and analyze" calls the UiManager callback, which
 * starts the FileService.
 */
class FileSelectionWindow : public UIElement {
public:
    /// Callback invoked with the path of the file to load.
    using LoadCallback = std::function<void(const std::string&)>; 
    
    /**
     * @param manager        Owning UiManager.
     * @param onLoadCallback Called when "Load and analyze" is pressed (if the path is not empty).
     */
    explicit FileSelectionWindow(UiManager* manager, LoadCallback onLoadCallback);
    /** @brief Draws the path field, the "Browse" button, the browser popup and "Load and analyze". */
    void draw() override;

private:
    UiManager* m_uiManager;         ///< Owner.
    LoadCallback m_onLoadCallback;  ///< Action of "Load and analyze".
    
    char m_filePathBuffer[260];     ///< Text of the path field (Windows MAX_PATH).

    ImGuiFileBrowser m_fileBrowser;     ///< File selection popup.
    bool m_showFileBrowser = false;     ///< true while the popup is open.
};