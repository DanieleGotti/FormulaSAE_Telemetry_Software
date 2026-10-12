/**
 * @file resources.hpp
 * @brief Old resource path utilities (legacy code).
 *
 * @deprecated Replaced by IAssetManager / CreateAssetManager(). getResourcesPath()
 *             always returns an empty string.
 */
#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <utility>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h> // for CFBundle
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace resources {
    /** @brief Resources folder (currently always ""). */
    std::string getResourcesPath();
    /**
     * @brief Full path of a resource.
     * @param relativePath Relative path.
     * @return getResourcesPath() + relativePath, or "../external/" + relativePath when the folder is empty.
     */
    std::string getResourcePath(const std::string& relativePath);
}