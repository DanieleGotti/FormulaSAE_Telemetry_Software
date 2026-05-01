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
#include <stdlib.h>
#endif

// Ricerca intelligente della cartella output_data
static std::filesystem::path GetDefaultPath() {
    std::filesystem::path exeDir;

    #ifdef _WIN32
        char buffer[MAX_PATH];
        if (GetModuleFileNameA(NULL, buffer, MAX_PATH) != 0) {
            exeDir = std::filesystem::path(buffer).parent_path();
        }
    #elif __APPLE__
        char buffer[PATH_MAX];
        uint32_t size = sizeof(buffer);
        if (_NSGetExecutablePath(buffer, &size) == 0) {
            char real_path[PATH_MAX];
            if (realpath(buffer, real_path) != NULL) {
                exeDir = std::filesystem::path(real_path).parent_path();
            } else {
                exeDir = std::filesystem::path(buffer).parent_path();
            }
        }
    #endif

    if (exeDir.empty()) {
        exeDir = std::filesystem::current_path();
    }

    // Cerca la cartella "output_data" salendo di livello 
    std::filesystem::path searchPath = exeDir;
    for (int i = 0; i < 4; ++i) {
        std::filesystem::path target = searchPath / "output_data";
        if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
            return target;
        }
        if (searchPath.has_parent_path()) {
            searchPath = searchPath.parent_path();
        } else {
            break;
        }
    }

    // Se non la trova la impostiamo accanto all'eseguibile per poi fargliela creare in automatico
    #ifdef __APPLE__
    // Su Mac, se siamo dentro un bundle (.app/Contents/MacOS), la mettiamo di fianco all'App
    if (exeDir.string().find(".app/Contents/MacOS") != std::string::npos) {
        return exeDir.parent_path().parent_path().parent_path() / "output_data";
    }
    #endif

    return exeDir / "output_data";
}

ImGuiFileBrowser::ImGuiFileBrowser() {
    m_currentPath = GetDefaultPath();

    // Se la cartella non esiste la crea
    if (!std::filesystem::exists(m_currentPath)) {
        try {
            std::filesystem::create_directories(m_currentPath);
            std::cout << "INFO [FileBrowser]: Cartella output_data creata automaticamente in: " << m_currentPath.string() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ATTENZIONE [FileBrowser]: Impossibile creare output_data: " << e.what() << std::endl;
            // Fallback di emergenza sicuro per Mac e Win
            #ifdef _WIN32
            m_currentPath = "C:\\";
            #else
            m_currentPath = "/";
            #endif
        }
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
                std::transform(extension.begin(), extension.end(), extension.begin(),[](unsigned char c){ return std::tolower(c); });
                // Mostriamo solo .csv e .bin
                if (extension == ".csv" || extension == ".bin") {
                    m_currentFiles.push_back({entry.path().filename().string(), false});
                }
            }
        }
        std::sort(m_currentFiles.begin(), m_currentFiles.end(),[](const FileEntry& a, const FileEntry& b){
            if (a.is_directory != b.is_directory) return a.is_directory;
            return a.name < b.name;
        });
    } catch(const std::exception& e) {
        std::cerr << "ERRORE[FileBrowser]: Impossibile accedere a " << m_currentPath.string() << ". " << e.what() << std::endl;
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
            ImGui::TextDisabled("Nessuna cartella o file compatibile trovato.");
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
#ifdef _WIN32
        strncpy_s(selected_buf, sizeof(selected_buf), m_selectedFile.c_str(), _TRUNCATE);
#else
        strncpy(selected_buf, m_selectedFile.c_str(), sizeof(selected_buf) - 1);
        selected_buf[sizeof(selected_buf) - 1] = '\0';
#endif
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