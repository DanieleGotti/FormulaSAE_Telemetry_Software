/**
 * @file ImGuiFileBrowser.cpp
 * @brief Implementation of the modal file browser (see ImGuiFileBrowser.hpp).
 */
#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <utils/fsUtils.hpp>
#include "UI/ImGuiFileBrowser.hpp"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#endif

namespace fs = std::filesystem;

/// Initial and "Home" folder: <executable folder>/output_data.
static fs::path GetDefaultPath() {
    // Rely on the OS-agnostic utility to find the executable folder
    return fsutils::getExecutableDirectory() / "output_data";
}

ImGuiFileBrowser::ImGuiFileBrowser() {
    m_currentPath = GetDefaultPath();

    // Create the folder if it does not exist (on error fall back to the working directory)
    if (!fs::exists(m_currentPath)) {
        try {
            fs::create_directories(m_currentPath);
            std::cout << "INFO [FileBrowser]: Folder output_data created automatically in: "
                      << m_currentPath.string() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "WARNING [FileBrowser]: Failed to create output_data folder: " << e.what() << std::endl;
            m_currentPath = fs::current_path();
        }
    }

    refreshDrives();
    refreshFiles();
    syncPathBuffer();
}

bool ImGuiFileBrowser::isFilesystemRoot(const fs::path& path) {
    // Covers both "C:\" on Windows and "/" on POSIX
    return path == path.root_path();
}

void ImGuiFileBrowser::syncPathBuffer() {
    const std::string text = m_currentPath.string();
    std::snprintf(m_pathBuffer, sizeof(m_pathBuffer), "%s", text.c_str());
}

void ImGuiFileBrowser::refreshDrives() {
    m_drives.clear();
#ifdef _WIN32
    // Drive bitmask: bit 0 = A:, bit 1 = B:, ...
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (mask & (1u << i)) {
            m_drives.push_back(std::string(1, static_cast<char>('A' + i)) + ":\\");
        }
    }
#else
    m_drives.push_back("/");
    const char* home = std::getenv("HOME");
    if (home) m_drives.push_back(home);
#endif
}

void ImGuiFileBrowser::refreshFiles() {
    m_currentFiles.clear();
    m_selectedFile.clear();

    for (const auto& entry : fs::directory_iterator(m_currentPath, fs::directory_options::skip_permission_denied)) {
        std::error_code ec;
        if (entry.is_directory(ec)) {
            m_currentFiles.push_back({ entry.path().filename().string(), true });
        } else if (entry.is_regular_file(ec)) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            // Only .csv and .bin
            if (extension == ".csv" || extension == ".bin") {
                m_currentFiles.push_back({ entry.path().filename().string(), false });
            }
        }
    }

    // Folders first, then files; each group in alphabetical order
    std::sort(m_currentFiles.begin(), m_currentFiles.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) return a.is_directory;
        return a.name < b.name;
    });
}

bool ImGuiFileBrowser::navigateTo(const fs::path& path) {
    std::error_code ec;
    fs::path target = fs::weakly_canonical(path, ec);
    if (ec || target.empty()) target = path;

    if (!fs::is_directory(target, ec)) {
        std::cerr << "ERROR [FileBrowser]: Not a readable folder: " << target.string() << std::endl;
        return false;
    }

    // On a read error go back to the previous folder
    const fs::path previous = m_currentPath;
    m_currentPath = target;
    try {
        refreshFiles();
    } catch (const std::exception& e) {
        std::cerr << "ERROR [FileBrowser]: Unable to access " << target.string() << ". " << e.what() << std::endl;
        m_currentPath = previous;
        try { refreshFiles(); } catch (...) {}
        syncPathBuffer();
        return false;
    }
    syncPathBuffer();
    return true;
}

std::optional<std::string> ImGuiFileBrowser::draw(const std::string& popup_id, bool& is_open) {
    std::optional<std::string> result = std::nullopt;

    if (is_open) {
        ImGui::OpenPopup(popup_id.c_str());
    }

    // Centered popup, 760x540 when it opens
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(popup_id.c_str(), &is_open, ImGuiWindowFlags_None)) {

        // Navigation requested this frame, applied only after the list is drawn
        // so the entry vector is never mutated while it is being iterated
        std::optional<fs::path> pendingNavigation;

        // ---- Toolbar: Up / Home / Refresh ----
        const bool atRoot = isFilesystemRoot(m_currentPath);
        if (atRoot) ImGui::BeginDisabled();
        if (ImGui::Button("Up")) {
            pendingNavigation = m_currentPath.parent_path();
        }
        if (atRoot) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            pendingNavigation = GetDefaultPath();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            pendingNavigation = m_currentPath;
            refreshDrives();
        }

        // ---- Drives / mount points ----
        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        for (const auto& drive : m_drives) {
            ImGui::SameLine();
            if (ImGui::Button(drive.c_str())) {
                pendingNavigation = fs::path(drive);
            }
        }

        // ---- Editable path bar (Enter or "Go") ----
        ImGui::SetNextItemWidth(-90.0f);
        const bool submitted = ImGui::InputText("##PathBar", m_pathBuffer, sizeof(m_pathBuffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Go", ImVec2(70, 0)) || submitted) {
            pendingNavigation = fs::path(m_pathBuffer);
        }

        ImGui::Separator();

        // ---- Folder and file list ----
        ImGui::BeginChild("File view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2));

        if (m_currentFiles.empty()) {
            ImGui::TextDisabled("No compatible folders or files found.");
        }

        for (const auto& entry : m_currentFiles) {
            const std::string label = (entry.is_directory ? "[Folder] " : "[File]    ") + entry.name;
            if (ImGui::Selectable(label.c_str(), (!entry.is_directory && entry.name == m_selectedFile))) {
                if (entry.is_directory) {
                    pendingNavigation = m_currentPath / entry.name;
                } else {
                    m_selectedFile = entry.name;
                }
            }
            // Double clicking a folder is the habitual way to enter it
            if (entry.is_directory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                pendingNavigation = m_currentPath / entry.name;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // ---- Selected file and Cancel / Open buttons ----
        char selected_buf[512];
        std::snprintf(selected_buf, sizeof(selected_buf), "%s", m_selectedFile.c_str());
        ImGui::SetNextItemWidth(-150.0f);
        ImGui::InputText("##SelectedFile", selected_buf, sizeof(selected_buf), ImGuiInputTextFlags_ReadOnly);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(60, 0))) {
            is_open = false;
        }
        ImGui::SameLine();
        if (m_selectedFile.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Open", ImVec2(60, 0))) {
            result = (m_currentPath / m_selectedFile).string();
            is_open = false;
        }
        if (m_selectedFile.empty()) ImGui::EndDisabled();

        // Safe point to change folder: nothing is iterating the vectors now
        if (pendingNavigation.has_value()) {
            navigateTo(*pendingNavigation);
        }

        ImGui::EndPopup();
    }

    return result;
}
