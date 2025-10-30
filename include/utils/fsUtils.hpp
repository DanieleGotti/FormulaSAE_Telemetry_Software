#include <filesystem>
#include <iostream>

namespace fsutils {
  std::filesystem::path getAppDocumentsDirectory();

  bool createAppDocumentsDirectory();
}