/**
 * @file resources.cpp
 * @brief Implementation of the old resource path utilities (legacy, see resources.hpp).
 */
#include <utils/resources.hpp>

#ifdef WIN32
#include "../assets/resources.h"
#endif

namespace resources {

std::string getResourcesPath() {
    return "";
}

// Joins the resources folder and the relative path
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