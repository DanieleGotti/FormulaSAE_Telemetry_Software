#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <filesystem>
#include "UI/ImGuiFileBrowser.hpp"

#ifdef _WIN32
#include <windows.h> 
#elif __APPLE__
#include <mach-o/dyld.h>
#include <limits.h> 
#endif

static std::filesystem::path GetDefaultPath() {
    #ifdef _WIN32
        char buffer[MAX_PATH];
        // Ottiene il percorso completo dell'eseguibile 
        if (GetModuleFileNameA(NULL, buffer, MAX_PATH) != 0) {
            return std::filesystem::path(buffer).parent_path().parent_path() / "output_data";
        }
    #elif __APPLE__
        char buffer[PATH_MAX];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) == 0) {
            return std::filesystem::path(buffer).parent_path().parent_path() / "output_data";
        }
    #endif

    return std::filesystem::current_path();
}

ImGuiFileBrowser::ImGuiFileBrowser() {
    m_currentPath = GetDefaultPath();

    if (!std::filesystem::exists(m_currentPath)) {
        std::cerr << "ATTENZIONE [FileBrowser]: La cartella di default non esiste: " << m_currentPath.string() << "." << std::endl;
        m_currentPath = "C:\\";
    }
    refreshFiles();
}

void ImGuiFileBrowser::refreshFiles() {
    m_currentFiles.clear();
    m_selectedFile.clear();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
            if (entry.is_directory()) {
                m_currentFiles.push_back({entry.path().filename().string(), true});
            } else if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c){ return std::tolower(c); });
                if (extension == ".txt") {
                    m_currentFiles.push_back({entry.path().filename().string(), false});
                }
            }
        }
        std::sort(m_currentFiles.begin(), m_currentFiles.end(), [](const FileEntry& a, const FileEntry& b){
            if (a.is_directory != b.is_directory) return a.is_directory;
            return a.name < b.name;
        });
    } catch(const std::exception& e) {
        std::cerr << "ERRORE [FileBrowser]: Impossibile accedere a " << m_currentPath.string() << "." << e.what() << std::endl;
        if (m_currentPath.has_parent_path()) {
            m_currentPath = m_currentPath.parent_path();
            refreshFiles();
        }
    }
}

std::optional<std::string> ImGuiFileBrowser::draw(const std::string& popup_id, bool& is_open) {
    std::optional<std::string> result = std::nullopt;

    if (is_open) {
        ImGui::OpenPopup(popup_id.c_str());
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(popup_id.c_str(), &is_open, ImGuiWindowFlags_None)) {
        
        if (m_currentPath.has_parent_path()) {
            if (ImGui::Button("Indietro")) {
                m_currentPath = m_currentPath.parent_path();
                refreshFiles();
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Indietro");
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::Text("Percorso: %s", m_currentPath.string().c_str());
        ImGui::Separator();

        ImGui::BeginChild("FileView", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2));
        
        if (m_currentFiles.empty()) {
            ImGui::TextDisabled("Nessuna cartella o file .txt trovati.");
        }

        for (const auto& entry : m_currentFiles) {
            std::string label = (entry.is_directory ? "[Cartella] " : "[File]    ") + entry.name;
            if (ImGui::Selectable(label.c_str(), (entry.name == m_selectedFile))) {
                if (entry.is_directory) {
                    m_currentPath /= entry.name;
                    refreshFiles();
                } else {
                    m_selectedFile = entry.name;
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        
        char selected_buf[260];
        strncpy_s(selected_buf, sizeof(selected_buf), m_selectedFile.c_str(), _TRUNCATE);
        ImGui::InputText("##SelectedFile", selected_buf, sizeof(selected_buf), ImGuiInputTextFlags_ReadOnly);
        
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 140);

        if (ImGui::Button("Annulla", ImVec2(60, 0))) {
            is_open = false;
        }
        ImGui::SameLine();
        if (m_selectedFile.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Apri", ImVec2(60, 0))) {
            result = (m_currentPath / m_selectedFile).string();
            is_open = false;
        }
        if (m_selectedFile.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }
    
    return result;
}