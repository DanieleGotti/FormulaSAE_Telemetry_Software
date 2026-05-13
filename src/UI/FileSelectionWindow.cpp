#include <imgui.h>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/FileSelectionWindow.hpp"

FileSelectionWindow::FileSelectionWindow(UiManager* manager, LoadCallback onLoadCallback)
    : m_uiManager(manager),
      m_onLoadCallback(std::move(onLoadCallback)) 
{
    m_filePathBuffer[0] = '\0';
}

void FileSelectionWindow::draw() {
    ImGui::PushFont(m_uiManager->font_title);
    ImGui::Begin("Load from file");
    ImGui::PopFont();
    
    ImGui::Text("Log file path");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        m_showFileBrowser = true;
    }

    if (m_showFileBrowser) {
        auto selected_path = m_fileBrowser.draw("Select file", m_showFileBrowser);
        if (selected_path.has_value()) {
            snprintf(m_filePathBuffer, sizeof(m_filePathBuffer), "%s", selected_path.value().c_str());
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Load and analyze")) {
        if (m_onLoadCallback && m_filePathBuffer[0] != '\0') {
            m_onLoadCallback(std::string(m_filePathBuffer)); 
        }
    }

    ImGui::End();
}