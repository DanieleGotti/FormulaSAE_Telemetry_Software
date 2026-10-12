/**
 * @file ImGuiFileBrowser.hpp
 * @brief Modal file browser built with ImGui (shows only folders and .csv / .bin files).
 *
 * @details
 * Features: folder navigation (click or double click), Up/Home/Refresh buttons,
 * quick drive selection (Windows) or "/" and home folder (Linux/macOS),
 * editable path bar. It starts from the `output_data` folder next to the executable.
 */
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

/**
 * @brief Modal popup to select a telemetry file.
 */
class ImGuiFileBrowser {
public:
    /** @brief Starts from `<executable>/output_data` (created if missing). */
    ImGuiFileBrowser();

    /**
     * @brief Draws the popup (call it every frame while `is_open` is true).
     * @param popup_id        ImGui popup ID/title.
     * @param[in,out] is_open Set to false when the user closes, cancels or opens a file.
     * @return Full path of the chosen file in the frame "Open" is pressed, std::nullopt otherwise.
     */
    std::optional<std::string> draw(const std::string& popup_id, bool& is_open);

private:
    /**
     * @brief Enters the `path` folder.
     *
     * If the folder cannot be read it returns false and leaves the browser
     * untouched, so an unreadable directory never traps the user.
     *
     * @param path Destination folder.
     * @return true if navigation succeeded.
     */
    bool navigateTo(const std::filesystem::path& path);
    /** @brief Reloads the content of the current folder (folders first, then .csv/.bin, alphabetically). */
    void refreshFiles();
    /** @brief Reloads the list of drives/mount points. */
    void refreshDrives();
    /** @brief Copies the current path into the path bar. */
    void syncPathBuffer();
    /** @brief true if `path` is the root of the file system ("C:\" or "/"). */
    static bool isFilesystemRoot(const std::filesystem::path& path);

    std::filesystem::path m_currentPath;  ///< Folder being shown.
    std::string m_selectedFile;           ///< Name of the selected file (empty = none).

    /// List entry: name and type.
    struct FileEntry {
        std::string name;
        bool is_directory;
    };
    std::vector<FileEntry> m_currentFiles;  ///< Filtered content of the current folder.

    /// Editable path bar, lets any folder be pasted in directly.
    char m_pathBuffer[1024] = {0};

    /// Mounted volumes, so removable media and other drives are reachable.
    std::vector<std::string> m_drives;
};
