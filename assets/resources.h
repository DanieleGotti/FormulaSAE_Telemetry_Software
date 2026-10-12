/**
 * @file resources.h
 * @brief IDs of the Windows resources embedded in the executable (used by windows_resources.rc and AssetManager_Windows).
 *
 * @details
 * To add a resource: define a new unique ID here, add the matching line in
 * windows_resources.rc (`ID  RCDATA  "path"`) and map the file name to the
 * ID in AssetManager_Windows::getImage()/getFont().
 * The file is also included by the resource compiler (rc): keep only #defines in it.
 */
#ifndef RESOURCES_H_INCLUDED
#define RESOURCES_H_INCLUDED

// Font (RCDATA)
#define IDR_FONT_REGULAR 101
#define IDR_FONT_BOLD    102

// Executable icon (ICON) and PNG images (RCDATA)
#define IDI_ICON1        201
#define IDI_ICON_STEER   202
#define IDI_ICON_ERB     203
#define IDI_ICON_LOGO    204

// Default ImGui window layout (RCDATA)
#define IDR_IMGUI_CONFIG 301

#endif
