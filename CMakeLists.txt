cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
project(telemetry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # Find Vulkan (from system installed SDK)
# find_package(Vulkan REQUIRED) # Uncomment if used in the future

# Add GLFW as subdirectory
add_subdirectory(external/glfw)

# ---- IMGUI ----
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

# Include directories
include_directories(
    # ${Vulkan_INCLUDE_DIRS} # Uncomment if used
    external/glm
    external/glfw/include
)

# Fetch Jsoncons (JSON library)
include(FetchContent)
FetchContent_Declare(
  jsoncons
  GIT_REPOSITORY https://github.com/danielaparker/jsoncons.git
  GIT_TAG v0.177.0
)

set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "Disable jsoncons tests")
set(JSONCONS_BUILD_EXAMPLES OFF CACHE BOOL "Disable jsoncons examples")

FetchContent_MakeAvailable(jsoncons)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Gather source files
file(GLOB SRC_FILES
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

# OS-Specific Asset Managers
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

# --- Windows Specific Configuration ---
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows_resources.rc")
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE})
    add_definitions(-DUNICODE -D_UNICODE)


    # If building with MinGW (GCC on Windows), statically link standard libraries 
    # and hide the background console window (-mwindows)
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -mwindows")
    endif()
endif()

# Hide noisy output in MSVC
if (MSVC)
    add_compile_options(/nologo)
    add_compile_options(/FC-)

    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
endif()

# --- macOS Specific Configuration ---
if(APPLE)
    # Set the app as a macOS Bundle 
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
    )

    # Specify OpenGL framework for macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")

    # App Icon handling
    set(MACOSX_BUNDLE_ICON_FILE "macos_icon.icns")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
    )

    set(ICON_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/macos_icon.icns")
    set_source_files_properties(${ICON_SOURCE_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${PROJECT_NAME} PRIVATE ${ICON_SOURCE_PATH})

    # Images handling
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


    # Fonts handling
    set(FONT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Bold.ttf"
    )

    foreach(FONT_FILE ${FONT_SOURCES})
        get_filename_component(FONT_NAME ${FONT_FILE} NAME)
        set_source_files_properties(${FONT_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts/")
        target_sources(${PROJECT_NAME} PRIVATE ${FONT_FILE})
    endforeach()

    # Default ImGui Config handling
    set(IMGUI_CONFIG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/resources/imgui.ini")
    
    set_source_files_properties(${IMGUI_CONFIG_SOURCE} PROPERTIES 
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_CONFIG_SOURCE})
endif()