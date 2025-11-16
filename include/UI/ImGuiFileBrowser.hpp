#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

class ImGuiFileBrowser {
public:
    ImGuiFileBrowser();

    // Disegna la finestra di dialogo
    std::optional<std::string> draw(const std::string& popup_id, bool& is_open);

private:
    void refreshFiles();

    std::filesystem::path m_currentPath;
    std::string m_selectedFile;
    
    struct FileEntry {
        std::string name;
        bool is_directory;
    };
    std::vector<FileEntry> m_currentFiles;
};