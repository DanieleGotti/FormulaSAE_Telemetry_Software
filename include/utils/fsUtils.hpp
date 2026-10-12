/**
 * @file fsUtils.hpp
 * @brief Cross-platform system paths: application data folder and executable folder.
 */
#pragma once
#include <filesystem>
#include <iostream>

namespace fsutils {
  /**
   * @brief Path of a file inside the application data folder.
   * @param relativePath Relative path (e.g. "imgui.ini").
   * @return getAppDocumentsDirectory() / relativePath.
   */
  std::filesystem::path getAppDocumentsFilePath(std::string relativePath);
  /**
   * @brief Application data folder (settings, window layout).
   *
   * - Windows: `%APPDATA%/telemetry_manager`
   * - macOS: `~/Library/Application Support/telemetry_manager`
   * - Linux: `$XDG_CONFIG_HOME/telemetry_manager` or `~/.config/telemetry_manager`
   *
   * The result is computed once and cached. The folder is not created
   * (see createAppDocumentsDirectory()).
   */
  std::filesystem::path getAppDocumentsDirectory();
  /**
   * @brief Folder containing the executable (on macOS: the folder containing the .app bundle).
   *
   * It is the base of the `output_data` folder where logs and exports are saved.
   * If it cannot be determined, the current working directory is returned.
   */
  std::filesystem::path getExecutableDirectory();

  /** @brief Creates the application data folder if it does not exist. @return always true. */
  bool createAppDocumentsDirectory();
}