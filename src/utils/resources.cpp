#include <utils/resources.hpp>

#ifdef WIN32
#include "../assets/resources.h"
#endif

namespace resources {

std::string getResourcesPath() {
    return "";
}

// Helper function for combining the resource path
std::string getResourcePath(const std::string& relativePath) {
    std::string resourcesPath = getResourcesPath();
    if (!resourcesPath.empty()) {
        if (resourcesPath.back() != '/') {
            resourcesPath += "/";
        }
        return resourcesPath + relativePath;
    }
    return "../external/" + relativePath; 
}
}