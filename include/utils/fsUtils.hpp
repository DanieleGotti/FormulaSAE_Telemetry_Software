#include <filesystem>
#include <iostream>

namespace fsutils {
  std::filesystem::path getAppDocumentsFilePath(std::string relativePath);
  std::filesystem::path getAppDocumentsDirectory();
  std::filesystem::path getExecutableDirectory();

  bool createAppDocumentsDirectory();
}