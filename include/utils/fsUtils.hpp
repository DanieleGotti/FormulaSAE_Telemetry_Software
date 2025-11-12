#include <filesystem>
#include <iostream>

namespace fsutils {
  std::filesystem::path getAppDocumentsFilePath(std::string relativePath);
  std::filesystem::path getAppDocumentsDirectory();

  bool createAppDocumentsDirectory();
}