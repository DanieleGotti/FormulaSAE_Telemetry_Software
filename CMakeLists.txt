cmake_minimum_required(VERSION 3.15)
project(telemetry_vulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # Trova Vulkan (dall'SDK installato sul sistema)
# find_package(Vulkan REQUIRED)

# Aggiungi GLFW come subdir (verrà compilato insieme al progetto)
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

target_link_libraries(imgui PUBLIC glfw) # usa glfw che già hai

# Include directories
include_directories(
    ${Vulkan_INCLUDE_DIRS}
    external/glm
    external/glfw/include
)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Sorgenti
file(GLOB SRC_FILES
    src/*.cpp
    src/Renderer/*.cpp
    src/UI/*.cpp
    src/Telemetry/*.cpp
    src/Telemetry/data_acquisition/*.cpp
    src/Telemetry/Services/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Link
target_link_libraries(${PROJECT_NAME} PRIVATE
    # Vulkan::Vulkan
    glfw
    imgui
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")
endif()