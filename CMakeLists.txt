# =============================================================================
#  ERB Telemetry Software - build configuration (CMake)
#
#  Typical build (full instructions in DEVELOPER_GUIDE.md):
#      git submodule update --init external/glfw external/glm external/imgui external/implot
#      cmake -S . -B build
#      cmake --build build
#
#  Dependencies:
#    - external/glfw, external/imgui (docking branch), external/implot, external/glm:
#      git submodules;
#    - external/stb_image/stb_image.h: single header included by the UI sources;
#    - jsoncons: downloaded automatically from GitHub with FetchContent.
#
#  Every .cpp in the folders listed in SRC_FILES is compiled automatically:
#  adding a file to an existing folder does not require editing this file.
#  For a NEW folder, add it to SRC_FILES.
#
#  Resources (fonts, images, default ImGui layout) are shipped differently per
#  operating system (see the Windows / macOS / Linux sections at the bottom
#  and include/utils/IAssetManager.hpp).
# =============================================================================
cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
project(telemetry LANGUAGES CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # Vulkan (from the system installed SDK) - not used
# find_package(Vulkan REQUIRED) # uncomment if used in the future

# GLFW (windows, input, OpenGL context) built as a subproject
add_subdirectory(external/glfw)

# ---- IMGUI + IMPLOT: built into a static library called "imgui" ----
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/implot)

add_library(imgui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
)

target_link_libraries(imgui PUBLIC glfw)

# Common include directories
include_directories(
    # ${Vulkan_INCLUDE_DIRS} # uncomment if Vulkan is used
    external/glm
    external/glfw/include
)

# Jsoncons (JSON library, used by SettingsManager), downloaded at configure time
include(FetchContent)
FetchContent_Declare(
  jsoncons
  GIT_REPOSITORY https://github.com/danielaparker/jsoncons.git
  GIT_TAG v0.177.0
)

set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "Disable jsoncons tests")
set(JSONCONS_BUILD_EXAMPLES OFF CACHE BOOL "Disable jsoncons examples")

FetchContent_MakeAvailable(jsoncons)

# Project headers: #include paths start from include/ (e.g. "UI/UIManager.hpp")
include_directories(${CMAKE_SOURCE_DIR}/include)

# Gather the source files.
# CONFIGURE_DEPENDS makes the build re-scan these folders, so a newly added
# source file is picked up without having to re-run cmake by hand
file(GLOB SRC_FILES CONFIGURE_DEPENDS
    src/*.cpp
    src/Renderer/*.cpp
    src/UI/*.cpp
    src/Telemetry/*.cpp
    src/Telemetry/data_acquisition/*.cpp
    src/Telemetry/Services/*.cpp
    src/Telemetry/data_writing/*.cpp
    src/Telemetry/file_reading/*.cpp
    src/application_services/*.cpp
    src/utils/*.cpp
    src/utils/AssetManager/AssetManagerFactory.cpp
)

# OS-specific asset manager (only one per build)
if(WIN32)
    list(APPEND SRC_FILES src/utils/AssetManager/AssetManager_Windows.cpp)
elseif(APPLE)
    list(APPEND SRC_FILES src/utils/AssetManager/AssetManager_MacOS.cpp)
else()
    list(APPEND SRC_FILES src/utils/AssetManager/AssetManager_Linux.cpp)
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES})


target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    imgui
    jsoncons
)

# --- Windows specific configuration ---
# Fonts, images and imgui.ini are embedded in the executable through
# assets/windows_resources.rc (IDs defined in assets/resources.h)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows_resources.rc")
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE})
    add_definitions(-DUNICODE -D_UNICODE)


    # With MinGW (GCC on Windows): standard libraries linked statically
    # and no console window in the background (-mwindows)
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -mwindows")
    endif()
endif()

# Hide noisy MSVC output
if (MSVC)
    add_compile_options(/nologo)
    add_compile_options(/FC-)

    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
endif()

# --- macOS specific configuration ---
# The app is a .app bundle: resources are copied into Contents/Resources
# (images in Resources/images, fonts in Resources/fonts, imgui.ini in Resources).
# A new image or font must be added to the lists below.
if(APPLE)
    # The executable becomes a macOS bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
    )

    # macOS OpenGL framework
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")

    # Application icon
    set(MACOSX_BUNDLE_ICON_FILE "macos_icon.icns")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
    )

    set(ICON_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/macos_icon.icns")
    set_source_files_properties(${ICON_SOURCE_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${PROJECT_NAME} PRIVATE ${ICON_SOURCE_PATH})

    # Images
    set(IMAGES_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/steering_wheel.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/logo_erb.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/icon.png"
    )

    foreach(IMAGE_FILE ${IMAGES_SOURCES})
        get_filename_component(IMAGE_NAME ${IMAGE_FILE} NAME)
        set_source_files_properties(${IMAGE_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/images/")
        target_sources(${PROJECT_NAME} PRIVATE ${IMAGE_FILE})
    endforeach()


    # Fonts
    set(FONT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Bold.ttf"
    )

    foreach(FONT_FILE ${FONT_SOURCES})
        get_filename_component(FONT_NAME ${FONT_FILE} NAME)
        set_source_files_properties(${FONT_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts/")
        target_sources(${PROJECT_NAME} PRIVATE ${FONT_FILE})
    endforeach()

    # Default ImGui window layout (used on the first launch)
    set(IMGUI_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/resources/imgui.ini")
    
    set_source_files_properties(${IMGUI_CONFIG_SOURCE} PROPERTIES 
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_CONFIG_SOURCE})
endif()

# --- Linux specific configuration ---
if(UNIX AND NOT APPLE)
  add_compile_options(-march=native -mtune=native)
  # System OpenGL libraries (provided by mesa-libGL-devel or equivalent)
  find_package(OpenGL REQUIRED)

  # OpenGL and math library (often required on Linux)
  target_link_libraries(${PROJECT_NAME} PRIVATE 
    OpenGL::GL 
    m  # standard C math library
  )
  
  # Copy the assets folder into the build folder so the executable can find it
  # (Linux has no bundle like the macOS .app folders)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" 
       DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()