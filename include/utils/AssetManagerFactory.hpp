/**
 * @file AssetManagerFactory.hpp
 * @brief Factory creating the asset manager suited to the operating system being compiled for.
 */
#pragma once
#include <memory>
#include "IAssetManager.hpp"

/**
 * @brief Creates the IAssetManager implementation for the current OS (Windows, macOS or Linux).
 * @return Asset manager, or nullptr on unsupported platforms.
 */
std::shared_ptr<IAssetManager> CreateAssetManager();
