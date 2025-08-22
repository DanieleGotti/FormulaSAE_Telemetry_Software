cmake_minimum_required(VERSION 3.15)
project(telemetry_vulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trova Vulkan
find_package(Vulkan REQUIRED)

# Trova GLFW
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# Aggiungi include e sorgenti
include_directories(
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    external/ # per ImGui/ImPlot in futuro
)

file(GLOB SRC_FILES
    src/*.cpp
    src/Renderer/*.cpp
    src/UI/*.cpp
    src/Telemetry/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
    ${Vulkan_LIBRARIES}
    ${GLFW_LIBRARIES}
)
