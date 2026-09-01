#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

class ImGuiFileBrowser {
public:
    ImGuiFileBrowser();

    std::optional<std::string> draw(const std::string& popup_id, bool& is_open);

private:
    // Lists `path`. Returns false and leaves the browser untouched when the
    // folder cannot be read, so an unreadable directory never traps the user.
    bool navigateTo(const std::filesystem::path& path);
    void refreshFiles();
    void refreshDrives();
    void syncPathBuffer();
    static bool isFilesystemRoot(const std::filesystem::path& path);

    std::filesystem::path m_currentPath;
    std::string m_selectedFile;

    struct FileEntry {
        std::string name;
        bool is_directory;
    };
    std::vector<FileEntry> m_currentFiles;

    // Editable location bar, lets any folder be pasted in directly
    char m_pathBuffer[1024] = {0};

    // Mounted volumes, so removable media and other drives are reachable
    std::vector<std::string> m_drives;
};
