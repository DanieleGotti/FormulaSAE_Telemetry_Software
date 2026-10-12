/**
 * @file UIManager.cpp
 * @brief Implementation of the main window, render loop and state machine (see UIManager.hpp).
 */
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <implot.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cassert>
#include <utility>
#include <vector>
#include <GLFW/glfw3.h>
#include "UI/UIManager.hpp"
#include "UI/Theme.hpp"
#include "UI/SerialDeviceSelection.hpp"
#include "UI/FileSelectionWindow.hpp"
#include "UI/LogTerminal.hpp"
#include "UI/AccBrkWindow.hpp"
#include "UI/StatusWindow.hpp"
#include "UI/SteerWindow.hpp"
#include "UI/SuspensionWindow.hpp"
#include "UI/HallWindow.hpp"
#include "UI/VarianceWindow.hpp"
#include "UI/PlaybackControlsWindow.hpp"
#include "UI/ModulesBatteryWindow.hpp"
#include "UI/SpeedometerWindow.hpp"
#include "UI/DynamicsPowertrainWindow.hpp"
#include "UI/InverterDataWindow.hpp"
#include "UI/TemperatureWindow.hpp"
#include "UI/CreditsWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"
#include "utils/fsUtils.hpp"
#include "../external/stb_image/stb_image.h"

#ifdef WIN32
#include "../assets/resources.h" 
#include <windows.h>
#endif

/**
 * @brief Subscriber that accumulates every received row in a vector.
 * @note Not used by the current flow (leftover from a previous playback implementation).
 */
class PlaybackDataSubscriber : public IAggregatedDataSubscriber {
public:
    void onAggregatedDataReceived(const DbRow& dataRow) override {
        collectedData.push_back(dataRow);
    }
    std::vector<DbRow> collectedData;  ///< Collected rows.
};


UiManager::UiManager() {
    if (!glfwInit()) {
        std::cerr << "CRITICAL [UiManager]: Failed to initialize GLFW! Are you running via SSH without a DISPLAY?" << std::endl;
        exit(1);
    }
    // ---- OpenGL context ----
#ifdef __APPLE__
    // macOS requires a forward-compatible Core Profile 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    // Windows/Linux: default context, GLSL 1.20 shaders for maximum compatibility
    glsl_version = "#version 120";
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    // Borderless, transparent window for the splash screen (borders come back after the splash)
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);              
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    m_window = glfwCreateWindow(500, 500, "ERB - Telemetry Software", NULL, NULL);
    
    // Center the splash window on the primary monitor
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (primary) {
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        glfwSetWindowPos((GLFWwindow*)m_window, (mode->width - 500) / 2, (mode->height - 500) / 2);
    }

    glfwMakeContextCurrent((GLFWwindow*)m_window);
    glfwSwapInterval(1); // V-Sync

    // ---- ImGui / ImPlot contexts ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();

    // On the first launch apply the default window layout (assets/resources/imgui.ini)
    if (ServiceManager::getSettingsManager()->getFirstAccess()) {
      auto [pFileData, pFileLength] = ServiceManager::getAssetManager()->getDefaultImGuiIniFile();

      if (pFileData && pFileLength > 0) {
            ImGui::LoadIniSettingsFromMemory((const char*)pFileData, pFileLength);
            std::cout << "INFO [UiManager]: Default ImGui layout applied successfully." << std::endl;
        } else {
            std::cerr << "WARNING [UiManager]: Unable to load default layout." << std::endl;
        }
      ServiceManager::getSettingsManager()->setFirstAccess(false);
    }

    // The layout modified by the user is saved in the application data folder.
    // The string is static because ImGui only keeps the pointer.
    static const std::string path = fsutils::getAppDocumentsFilePath("imgui.ini").string();
    io.IniFilename = path.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // dockable windows
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // windows can be dragged outside the main window
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // The terminal must be created early: from here on cout/cerr end up in the UI
    m_logTerminal = std::make_shared<LogTerminal>(this);
    std::cout << "INFO [UiManager]: UiManager initialized." << std::endl;
    io.Fonts->Clear(); 

#ifdef WIN32
    SetWindowIconFromResource();
#endif

    // ---- Fonts: Regular for text and data, Bold for labels and titles ----
    // FontDataOwnedByAtlas = false: the memory stays owned by the asset manager
    auto [pFontDataRegular, dFontSizeRegular] = ServiceManager::getAssetManager()->getFont("RobotoCondensed-Regular.ttf");
    if (pFontDataRegular && dFontSizeRegular > 0) {
        ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
        font_body = io.Fonts->AddFontFromMemoryTTF(pFontDataRegular, dFontSizeRegular, ServiceManager::getSettingsManager()->getBodyFontSize(), &cfg);
        font_data = io.Fonts->AddFontFromMemoryTTF(pFontDataRegular, dFontSizeRegular, ServiceManager::getSettingsManager()->getDataFontSize(), &cfg);
    } else { io.Fonts->AddFontDefault(); }
    auto [pFontDataBold, dFontSizeBold] = ServiceManager::getAssetManager()->getFont("RobotoCondensed-Bold.ttf");
    if (pFontDataBold && dFontSizeBold > 0) {
        ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
        font_label = io.Fonts->AddFontFromMemoryTTF(pFontDataBold, dFontSizeBold, ServiceManager::getSettingsManager()->getLabelFontSize(), &cfg);
        font_title = io.Fonts->AddFontFromMemoryTTF(pFontDataBold, dFontSizeBold, ServiceManager::getSettingsManager()->getTitleFontSize(), &cfg);
    } else { io.Fonts->AddFontDefault(); }

    ImGui::GetIO().FontGlobalScale = ServiceManager::getSettingsManager()->getGlobalFontScale();
    assert(font_body != nullptr && "ERROR [UiManager]: Failed to load fonts.");
    std::cout << "INFO [UiManager]: Fonts loaded." << std::endl;

    // ---- Theme saved in the settings ----
    m_isDarkTheme = ServiceManager::getSettingsManager()->getDarkMode();
    ApplyTheme(ImGui::GetStyle(), m_isDarkTheme);

    // ---- ImGui platform and renderer backends ----
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    setupInitialState();
}

UiManager::~UiManager() {
    // Windows must be unsubscribed before being destroyed, otherwise the
    // DataManager would call objects that no longer exist
    if (ServiceManager::getDataManager()) {
        if (m_dashboard) ServiceManager::getDataManager()->removeSubscriber(m_dashboard.get());
        if (m_accBrkWindow) ServiceManager::getDataManager()->removeSubscriber(m_accBrkWindow.get());
        if (m_statusWindow) ServiceManager::getDataManager()->removeSubscriber(m_statusWindow.get());
        if (m_steerWindow) ServiceManager::getDataManager()->removeSubscriber(m_steerWindow.get());
        if (m_suspensionWindow) ServiceManager::getDataManager()->removeSubscriber(m_suspensionWindow.get());
        if (m_hallWindow) ServiceManager::getDataManager()->removeSubscriber(m_hallWindow.get());
        if (m_modulesBatteryWindow) ServiceManager::getDataManager()->removeSubscriber(m_modulesBatteryWindow.get());
        if (m_varianceWindow) ServiceManager::getDataManager()->removeSubscriber(m_varianceWindow.get());
        if (m_speedometerWindow) ServiceManager::getDataManager()->removeSubscriber(m_speedometerWindow.get());
        if (m_dynamicsWindow) ServiceManager::getDataManager()->removeSubscriber(m_dynamicsWindow.get());
        if (m_inverterWindow) ServiceManager::getDataManager()->removeSubscriber(m_inverterWindow.get());
        if (m_temperatureWindow) ServiceManager::getDataManager()->removeSubscriber(m_temperatureWindow.get());
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow((GLFWwindow*)m_window);
    glfwTerminate();
}

void* UiManager::getWindowHandle() {
    return m_window;
}

AppState UiManager::getCurrentState() const {
    return m_currentState;
}

void UiManager::setupInitialState() {
    if (m_isFirstBoot) {
        // Only on the very first launch: splash screen with the logo
        m_currentState = AppState::SPLASH_SCREEN;
        loadTextureFromMemory("icon.png", &m_splashTextureID, &m_splashWidth, &m_splashHeight);
        m_splashStartTime = glfwGetTime();
        m_isFirstBoot = false;
    } else {
        m_currentState = AppState::CONFIGURING;
    }

    // "Connect" button callback: starts the serial port and switches to live
    auto onConnect = [this](const std::string& port, int baudrate) {
        ServiceManager::setAcquisitionMethod(ACQUISITION_METHOD_SERIAL);
        if (ServiceManager::configureSerial(port, baudrate) && ServiceManager::startServices()) {
            removeElement(m_serialSelectionWindow);
            removeElement(m_fileSelectionWindow);
            m_serialSelectionWindow = nullptr;
            m_fileSelectionWindow = nullptr;
            transitionToConnectedState(AppState::CONNECTED_LIVE);
        } else {
            std::cerr << "ERROR [UiManager]: Live connection failed." << std::endl;
        }
    };

    // "Load and analyze" button callback: starts loading in the background
    auto onLoadFile = [this](const std::string& filePath) {
        ServiceManager::setAcquisitionMethod(ACQUISITION_METHOD_FILE);
        ServiceManager::configureFile(filePath);
        ServiceManager::startServices(); 

        removeElement(m_serialSelectionWindow);
        removeElement(m_fileSelectionWindow);
        m_serialSelectionWindow = nullptr;
        m_fileSelectionWindow = nullptr;
        m_currentState = AppState::LOADING_FILE;
    };
    
    // Home screen windows
    auto serialWin = std::make_unique<SerialDeviceSelection>(this, onConnect);
    m_serialSelectionWindow = serialWin.get();
    addElement(std::move(serialWin));

    auto fileWin = std::make_unique<FileSelectionWindow>(this, onLoadFile);
    m_fileSelectionWindow = fileWin.get();
    addElement(std::move(fileWin));

    auto creditsWin = std::make_unique<CreditsWindow>(this);
    addElement(std::move(creditsWin));
}

void UiManager::transitionToConnectedState(AppState connectedState) {
    m_currentState = connectedState;

    // Create every data window (both live and playback)
    m_dashboard = std::make_shared<Dashboard>(this);
    m_accBrkWindow = std::make_shared<AccBrkWindow>(this);
    m_statusWindow = std::make_shared<StatusWindow>(this);
    m_steerWindow = std::make_shared<SteerWindow>(this);
    m_suspensionWindow = std::make_shared<SuspensionWindow>(this);
    m_hallWindow = std::make_shared<HallWindow>(this);
    m_modulesBatteryWindow = std::make_shared<ModulesBatteryWindow>(this);
    m_varianceWindow = std::make_shared<VarianceWindow>(this);
    m_speedometerWindow = std::make_shared<SpeedometerWindow>(this);
    m_dynamicsWindow = std::make_shared<DynamicsPowertrainWindow>(this);
    m_inverterWindow = std::make_shared<InverterDataWindow>(this);
    m_temperatureWindow = std::make_shared<TemperatureWindow>(this);

    if (connectedState == AppState::CONNECTED_LIVE) {
        // Live: windows receive the data through DataManager callbacks
        auto dataManager = ServiceManager::getDataManager();
        dataManager->addSubscriber(m_dashboard.get());
        dataManager->addSubscriber(m_accBrkWindow.get());
        dataManager->addSubscriber(m_statusWindow.get());
        dataManager->addSubscriber(m_steerWindow.get());
        dataManager->addSubscriber(m_suspensionWindow.get());
        dataManager->addSubscriber(m_hallWindow.get());
        dataManager->addSubscriber(m_modulesBatteryWindow.get());
        dataManager->addSubscriber(m_varianceWindow.get());
        dataManager->addSubscriber(m_speedometerWindow.get());
        dataManager->addSubscriber(m_dynamicsWindow.get());
        dataManager->addSubscriber(m_inverterWindow.get());
        dataManager->addSubscriber(m_temperatureWindow.get());
    } else if (connectedState == AppState::CONNECTED_PLAYBACK) {
        // Playback: windows read from the PlaybackManager; the slider is needed
        m_playbackControls = std::make_shared<PlaybackControlsWindow>(this);
    }
}

void UiManager::run() {
    while (!glfwWindowShouldClose((GLFWwindow*)m_window)) {
        // 1. Events and new frame
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // 2. Build the interface
        ImGui::PushFont(font_body);
        if (m_currentState != AppState::SPLASH_SCREEN) {
            showDockingSpace(); 
        }
        draw();
        ImGui::PopFont();
        
        // 3. Rendering
        ImGui::Render();
        
        int display_w, display_h;
        glfwGetFramebufferSize((GLFWwindow*)m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        // Transparent background during the splash, theme colour otherwise
        ImVec4 clear_color;
        if (m_currentState == AppState::SPLASH_SCREEN) {
            clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); 
        } else {
            clear_color = m_isDarkTheme ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        }
        
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
        // Windows detached from the main one (multi-viewport)
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // 4. Present and perform scheduled removals (frame completed)
        glfwSwapBuffers((GLFWwindow*)m_window);
        processPendingRemovals();
    }

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
}

void UiManager::draw() {

    // ---- Splash screen: centered logo, then switch to the normal window ----
    if (m_currentState == AppState::SPLASH_SCREEN) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::Begin("Splash screen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
        
        if (m_splashTextureID && m_splashWidth > 0) {
            // Logo at most 400 px wide
            float scale = 1.0f;
            if (m_splashWidth > 400) scale = 400.0f / m_splashWidth;
            
            ImVec2 img_size(m_splashWidth * scale, m_splashHeight * scale);
            ImVec2 pos = ImVec2((viewport->Size.x - img_size.x) * 0.5f, (viewport->Size.y - img_size.y) * 0.5f);
            
            ImGui::SetCursorPos(pos);
            ImGui::Image((ImTextureID)(intptr_t)m_splashTextureID, img_size);
        }
        ImGui::End();

        // After 4 seconds: decorated, maximised window, CONFIGURING state
        if (glfwGetTime() - m_splashStartTime > 4.0) {
            
            glfwHideWindow((GLFWwindow*)m_window);
            m_currentState = AppState::CONFIGURING; 
        
            glfwSetWindowAttrib((GLFWwindow*)m_window, GLFW_DECORATED, GLFW_TRUE);

#if defined(__APPLE__)
            // On macOS maximising does not work well with the way the splash is handled:
            // the window is set manually to the monitor work area
            int xpos, ypos, width, height;
            glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &xpos, &ypos, &width, &height);
            glfwSetWindowPos((GLFWwindow*)m_window, xpos, ypos);
            glfwSetWindowSize((GLFWwindow*)m_window, width, height);
#else
            glfwMaximizeWindow((GLFWwindow*)m_window);
#endif
            glfwShowWindow((GLFWwindow*)m_window);

            if (m_splashTextureID) {
                glDeleteTextures(1, &m_splashTextureID); 
                m_splashTextureID = 0;
            }
        }
        return; 
    }

    if (m_logTerminal) {
        m_logTerminal->draw();
    }

    // ---- File loading: wait for FileService, then switch to playback ----
    if (m_currentState == AppState::LOADING_FILE) {
        auto fileService = ServiceManager::getFileService();
        if (fileService && fileService->isFinished()) {
            
            auto loadedData = fileService->getResult();
            std::string originalExt = fileService->getConfig().filePath;
            originalExt = originalExt.substr(originalExt.find_last_of("."));
            std::transform(originalExt.begin(), originalExt.end(), originalExt.begin(), ::tolower);
            
            // The dataset is handed (moved, no copies) to the PlaybackManager
            ServiceManager::getPlaybackManager()->setDataset(std::move(loadedData), originalExt);
            transitionToConnectedState(AppState::CONNECTED_PLAYBACK);

        } else {
            ImGui::Begin("Loading in progress");
            ImGui::Text("Analyzing telemetry file and calculating multi-threading stats. Please wait.");
            ImGui::End();
        }
    }

    // ---- Generic windows (configuration, credits) ----
    for (auto& element : m_uiElements) {
        element->draw();
    }
    
    // ---- Data windows ----
    if (m_currentState == AppState::CONNECTED_LIVE || m_currentState == AppState::CONNECTED_PLAYBACK) {
        if (m_dashboard) m_dashboard->draw();
        if (m_accBrkWindow) m_accBrkWindow->draw();
        if (m_statusWindow) m_statusWindow->draw();
        if (m_steerWindow) m_steerWindow->draw();
        if (m_suspensionWindow) m_suspensionWindow->draw();
        if (m_hallWindow) m_hallWindow->draw();
        if (m_modulesBatteryWindow) m_modulesBatteryWindow->draw();
        if (m_varianceWindow) m_varianceWindow->draw();
        if (m_speedometerWindow) m_speedometerWindow->draw();
        if (m_dynamicsWindow) m_dynamicsWindow->draw();
        if (m_inverterWindow) m_inverterWindow->draw();
        if (m_temperatureWindow) m_temperatureWindow->draw();
    }

    if (m_currentState == AppState::CONNECTED_PLAYBACK) {
        if (m_playbackControls) m_playbackControls->draw();
    }
}


void UiManager::addElement(std::unique_ptr<UIElement> uiElement) {
    m_uiElements.push_back(std::move(uiElement));
}

void UiManager::removeElement(UIElement* uiElement) {
    m_elementsToRemove.push_back(uiElement);
}

void UiManager::processPendingRemovals() {
    if (m_elementsToRemove.empty()) return;
    for (UIElement* elementToRemove : m_elementsToRemove) {
        m_uiElements.erase(
            std::remove_if(m_uiElements.begin(), m_uiElements.end(),
                [elementToRemove](const std::unique_ptr<UIElement>& elem) { return elem.get() == elementToRemove; }
            ),
            m_uiElements.end()
        );
    }
    m_elementsToRemove.clear();
}


void UiManager::showDockingSpace() {
    // Invisible full-screen window hosting the dockspace and the menu bar
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground; 

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Dock space host", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // ---- Menu bar ----
    if (ImGui::BeginMainMenuBar()) {
        
        // "Home" is available only outside the configuration screen
        if (m_currentState != AppState::CONFIGURING) {
            if (ImGui::MenuItem("Home")) {
                resetToHome();
            }
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::BeginMenu("Appearance")) {
                // Light/dark theme switch (saved in the settings)
                bool isDark = ServiceManager::getSettingsManager()->getDarkMode();
                if (ImGui::MenuItem(isDark ? "Light mode" : "Dark mode")) {
                    ServiceManager::getSettingsManager()->setDarkMode(!isDark);
                    m_isDarkTheme = !isDark;
                    ApplyTheme(ImGui::GetStyle(), !isDark);
                }
                ImGui::Separator();
                // Global text scale, 0.1 steps (minimum 0.5)
                ImGui::Text("Text size");
                ImGui::SameLine();
                float fontScale = ServiceManager::getSettingsManager()->getGlobalFontScale();
                bool changed = false;
                if (ImGui::Button("-")) { fontScale = std::max(0.5f, fontScale - 0.1f); changed = true; }
                ImGui::SameLine();
                if (ImGui::Button("+")) { fontScale += 0.1f; changed = true; }
                ImGui::SameLine();
                ImGui::Text("%.1fx", fontScale);
                if (changed) {
                    ServiceManager::getSettingsManager()->setGlobalFontScale(fontScale);
                    ImGui::GetIO().FontGlobalScale = fontScale;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::End();
}

void UiManager::resetToHome() {
    std::cout << "INFO [UiManager]: Resetting interface to Home screen." << std::endl;

    // 1. Unsubscribe the windows from the DataManager
    if (ServiceManager::getDataManager()) {
        if (m_dashboard) ServiceManager::getDataManager()->removeSubscriber(m_dashboard.get());
        if (m_accBrkWindow) ServiceManager::getDataManager()->removeSubscriber(m_accBrkWindow.get());
        if (m_statusWindow) ServiceManager::getDataManager()->removeSubscriber(m_statusWindow.get());
        if (m_steerWindow) ServiceManager::getDataManager()->removeSubscriber(m_steerWindow.get());
        if (m_suspensionWindow) ServiceManager::getDataManager()->removeSubscriber(m_suspensionWindow.get());
        if (m_hallWindow) ServiceManager::getDataManager()->removeSubscriber(m_hallWindow.get());
        if (m_modulesBatteryWindow) ServiceManager::getDataManager()->removeSubscriber(m_modulesBatteryWindow.get());
        if (m_varianceWindow) ServiceManager::getDataManager()->removeSubscriber(m_varianceWindow.get());
        if (m_speedometerWindow) ServiceManager::getDataManager()->removeSubscriber(m_speedometerWindow.get());
        if (m_dynamicsWindow) ServiceManager::getDataManager()->removeSubscriber(m_dynamicsWindow.get());
        if (m_inverterWindow) ServiceManager::getDataManager()->removeSubscriber(m_inverterWindow.get());
        if (m_temperatureWindow) ServiceManager::getDataManager()->removeSubscriber(m_temperatureWindow.get());
    }

    // 2. Stop and recreate the backend services
    ServiceManager::resetForNewSession();

    // 3. Destroy the data windows
    m_dashboard.reset();
    m_accBrkWindow.reset();
    m_statusWindow.reset();
    m_steerWindow.reset();
    m_suspensionWindow.reset();
    m_hallWindow.reset();
    m_modulesBatteryWindow.reset();
    m_playbackControls.reset();
    m_varianceWindow.reset();
    m_speedometerWindow.reset();
    m_dynamicsWindow.reset();
    m_inverterWindow.reset();
    m_temperatureWindow.reset();

    // 4. Recreate the Home screen
    m_uiElements.clear();
    m_serialSelectionWindow = nullptr;
    m_fileSelectionWindow = nullptr;

    setupInitialState();
}

bool UiManager::loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    auto [pData, dataSize] = ServiceManager::getAssetManager()->getImage(filename);
    if (!pData || dataSize == 0) return false;

    // Decode PNG -> RGBA (4 channels)
    int image_width = 0, image_height = 0;
    unsigned char* image_data = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(pData), dataSize, &image_width, &image_height, nullptr, 4
    );
    if (!image_data) return false;

    // Upload into an OpenGL texture
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture; 
    *out_width = image_width; 
    *out_height = image_height;
    return true;
}


#if defined(WIN32)
void UiManager::SetWindowIconFromResource() {
    // Load the IDI_ICON1 icon embedded in the executable
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    if (!hIcon) return;

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return;

    BITMAP bmpColor;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmpColor);
    int width = bmpColor.bmWidth;
    int height = bmpColor.bmHeight;

    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, iconInfo.hbmColor);

    // Extract the 32-bit pixels (negative height = top-down rows)
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<unsigned char> pixels(width * height * 4);
    GetDIBits(memDC, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    // Windows provides BGRA, GLFW wants RGBA: swap the R and B channels
  for (size_t i = 0; i < pixels.size(); i += 4) {
        unsigned char temp = pixels[i];       
        pixels[i] = pixels[i + 2];            
        pixels[i + 2] = temp;                 
    }
    
    GLFWimage img{width, height, pixels.data()};
    glfwSetWindowIcon((GLFWwindow*)m_window, 1, &img);

    // Release the GDI resources
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);
}
#endif