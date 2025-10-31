cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.15)
project(telemetry_vulkan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # Trova Vulkan (dall'SDK installato sul sistema)
# find_package(Vulkan REQUIRED) # Assicurati di scommentare se lo usi

# Aggiungi GLFW come subdir
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
    # ${Vulkan_INCLUDE_DIRS} # Assicurati di scommentare se lo usi
    external/glm
    external/glfw/include
)

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

# Sorgenti
file(GLOB SRC_FILES
    src/*.cpp
    src/Renderer/*.cpp
    src/UI/*.cpp
    src/Telemetry/*.cpp
    src/Telemetry/data_acquisition/*.cpp
    src/Telemetry/Services/*.cpp
    src/Telemetry/data_writing/*.cpp
    src/application_services/*.cpp
    src/utils/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})


target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    imgui
    jsoncons
)

# Sezione di configurazione specifica per Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)

    # Icona per Windows
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows_resources.rc")
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE})
endif()

# Sezione di configurazione specifica per macOS
# (Questa parte deve essere dopo add_executable(${PROJECT_NAME} ${SRC_FILES}))

# (Questa parte deve essere dopo add_executable(${PROJECT_NAME} ${SRC_FILES})
# e dopo aver impostato MACOSX_BUNDLE TRUE tramite set_target_properties,
# come suggerito nella mia revisione precedente del tuo CMakeLists.txt completo.)

if(APPLE)
    # Imposta che l'app è un bundle macOS (questo è fondamentale, deve essere presente e TRUE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
    )

    # Specifica il framework OpenGL per macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")

    # --- Gestione Icona ---
    # Il nome del file icona dentro il bundle
    set(MACOSX_BUNDLE_ICON_FILE "macos_icon.icns")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE ${MACOSX_BUNDLE_ICON_FILE}
    )

    # Il percorso del file icona sorgente
    set(ICON_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/macos_icon.icns")
    
    # Imposta la destinazione nel bundle e aggiungilo come "sorgente" al target
    set_source_files_properties(${ICON_SOURCE_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${PROJECT_NAME} PRIVATE ${ICON_SOURCE_PATH})


    # --- Gestione Font ---
    # Elenca i file dei font
    set(FONT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/RobotoCondensed-Bold.ttf"
    )

    # Itera su ogni font per impostare la proprietà e aggiungerlo singolarmente
    foreach(FONT_FILE ${FONT_SOURCES})
        # Se vuoi i font in una sottocartella "fonts" dentro Resources:
        # Estrai solo il nome del file per creare il percorso relativo
        get_filename_component(FONT_NAME ${FONT_FILE} NAME)
        set_source_files_properties(${FONT_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts/")
        
        # Aggiungilo come sorgente individuale
        target_sources(${PROJECT_NAME} PRIVATE ${FONT_FILE})
    endforeach()

    # Se avessi altre risorse (immagini, ecc.) potresti creare liste simili
    # e usare un foreach per gestirle singolarmente.

endif()