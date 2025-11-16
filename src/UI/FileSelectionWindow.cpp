#include <imgui.h>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/FileSelectionWindow.hpp"

FileSelectionWindow::FileSelectionWindow(UiManager* manager, LoadCallback onLoadCallback)
    : m_uiManager(manager),
      m_onLoadCallback(std::move(onLoadCallback)),
      m_shouldGenerateCsv(true)
{
    m_filePathBuffer[0] = '\0';
}

void FileSelectionWindow::draw() {
    ImGui::PushFont(m_uiManager->font_title);
    ImGui::Begin("Carica da file");
    ImGui::PopFont();
    
    ImGui::Text("Percorso file di log");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Sfoglia")) {
        m_showFileBrowser = true;
    }

    // Disegna la finestra del browser se il flag è attivo
    if (m_showFileBrowser) {
        auto selected_path = m_fileBrowser.draw("Scegli File", m_showFileBrowser);
        if (selected_path.has_value()) {
            // Se l'utente ha scelto un file, aggiorna il buffer
            snprintf(m_filePathBuffer, sizeof(m_filePathBuffer), "%s", selected_path.value().c_str());
        }
    }
    
    ImGui::Checkbox("Genera file .csv", &m_shouldGenerateCsv);
    ImGui::Separator();
    
    if (ImGui::Button("Carica e analizza")) {
        if (m_onLoadCallback && m_filePathBuffer[0] != '\0') {
            m_onLoadCallback(std::string(m_filePathBuffer), m_shouldGenerateCsv);
        }
    }

    ImGui::End();
}