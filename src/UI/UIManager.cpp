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
#include "UI/PlaybackControlsWindow.hpp"
#include "UI/TemperatureWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"
#include "utils/fsUtils.hpp"

#ifdef WIN32
#include "../assets/resources.h" 
#include <windows.h>
#endif

class PlaybackDataSubscriber : public IAggregatedDataSubscriber {
public:
    void onAggregatedDataReceived(const DbRow& dataRow) override {
        collectedData.push_back(dataRow);
    }
    std::vector<DbRow> collectedData;
};


UiManager::UiManager() {
    if (!glfwInit()) return;
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    m_window = glfwCreateWindow(1280, 720, "ERB - Telemetria", NULL, NULL);
    glfwMakeContextCurrent((GLFWwindow*)m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    static const std::string path = fsutils::getAppDocumentsFilePath("imgui.ini").string();
    io.IniFilename = path.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    m_logTerminal = std::make_shared<LogTerminal>(this);
    std::cout << "INFO [UiManager]: UiManager inizializzato." << std::endl;
    io.Fonts->Clear(); 

#ifdef WIN32
    SetWindowIconFromResource();
#endif

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
    assert(font_body != nullptr && "ERRORE [UiManager]: Impossibile caricare i font.");
    std::cout << "INFO [UiManager]: Font caricati." << std::endl;

    ApplyTheme(ImGui::GetStyle(), ServiceManager::getSettingsManager()->getDarkMode());
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    setupInitialState();
}

UiManager::~UiManager() {
    if (ServiceManager::getAggregator()) {
        if (m_dashboard) ServiceManager::getAggregator()->unsubscribe(m_dashboard.get());
        if (m_accBrkWindow) ServiceManager::getAggregator()->unsubscribe(m_accBrkWindow.get());
        if (m_statusWindow) ServiceManager::getAggregator()->unsubscribe(m_statusWindow.get());
        if (m_steerWindow) ServiceManager::getAggregator()->unsubscribe(m_steerWindow.get());
        if (m_suspensionWindow) ServiceManager::getAggregator()->unsubscribe(m_suspensionWindow.get());
        if (m_hallWindow) ServiceManager::getAggregator()->unsubscribe(m_hallWindow.get());
        if (m_temperatureWindow) ServiceManager::getAggregator()->unsubscribe(m_temperatureWindow.get());
        if (m_tempDataSubscriber) ServiceManager::getAggregator()->unsubscribe(m_tempDataSubscriber.get());
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
    m_currentState = AppState::CONFIGURING;

    auto onConnect = [this](const std::string& port, int baudrate) {
        ServiceManager::setAcquisitionMethod(ACQUISITION_METHOD_SERIAL);
        if (ServiceManager::configureSerial(port, baudrate) && ServiceManager::startServices()) {
            removeElement(m_serialSelectionWindow);
            removeElement(m_fileSelectionWindow);
            m_serialSelectionWindow = nullptr;
            m_fileSelectionWindow = nullptr;
            transitionToConnectedState(AppState::CONNECTED_LIVE);
        } else {
            std::cerr << "ERRORE [UiManager]: Connessione live fallita." << std::endl;
        }
    };

    auto onLoadFile = [this](const std::string& filePath, bool generateCsv) {
        if (generateCsv) {
            this->m_shouldGenerateCsvForPlayback = generateCsv;
        }
        
        ServiceManager::setAcquisitionMethod(ACQUISITION_METHOD_FILE);
        ServiceManager::configureFile(filePath);
        ServiceManager::startServices(); 

        removeElement(m_serialSelectionWindow);
        removeElement(m_fileSelectionWindow);
        m_serialSelectionWindow = nullptr;
        m_fileSelectionWindow = nullptr;
        m_currentState = AppState::LOADING_FILE;
    };
    
    auto serialWin = std::make_unique<SerialDeviceSelection>(this, onConnect);
    m_serialSelectionWindow = serialWin.get();
    addElement(std::move(serialWin));

    auto fileWin = std::make_unique<FileSelectionWindow>(this, onLoadFile);
    m_fileSelectionWindow = fileWin.get();
    addElement(std::move(fileWin));
}

void UiManager::transitionToConnectedState(AppState connectedState) {
    m_currentState = connectedState;

    m_dashboard = std::make_shared<Dashboard>(this);
    m_accBrkWindow = std::make_shared<AccBrkWindow>(this);
    m_statusWindow = std::make_shared<StatusWindow>(this);
    m_steerWindow = std::make_shared<SteerWindow>(this);
    m_suspensionWindow = std::make_shared<SuspensionWindow>(this);
    m_hallWindow = std::make_shared<HallWindow>(this);
    m_temperatureWindow = std::make_shared<TemperatureWindow>(this);

    if (connectedState == AppState::CONNECTED_LIVE) {
        // In modalità live, tutte le finestre si iscrivono all'aggregator
        auto aggregator = ServiceManager::getAggregator();
        aggregator->subscribe(m_dashboard.get());
        aggregator->subscribe(m_accBrkWindow.get());
        aggregator->subscribe(m_statusWindow.get());
        aggregator->subscribe(m_steerWindow.get());
        aggregator->subscribe(m_suspensionWindow.get());
        aggregator->subscribe(m_hallWindow.get());
        aggregator->subscribe(m_temperatureWindow.get());
    } else if (connectedState == AppState::CONNECTED_PLAYBACK) {
        // In modalità lettura file, viene creata la finestra dei controlli
        m_playbackControls = std::make_shared<PlaybackControlsWindow>(this);
    }
}

void UiManager::run() {
    while (!glfwWindowShouldClose((GLFWwindow*)m_window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::PushFont(font_body);
        showDockingSpace();
        draw();
        ImGui::PopFont();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers((GLFWwindow*)m_window);
        processPendingRemovals();
    }

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
}

void UiManager::draw() {
    if (m_logTerminal) {
        m_logTerminal->draw();
    }

    if (m_currentState == AppState::LOADING_FILE) {
        auto fileService = ServiceManager::getFileService();
        if (fileService && fileService->isFinished()) {
            
            auto loadedData = fileService->getResult();
            
            if (m_shouldGenerateCsvForPlayback && !loadedData.empty()) {
                
                std::filesystem::path inputPath(fileService->getConfig().filePath);
                std::string newFilename = "analysis_" + inputPath.stem().string() + ".csv";
                
                std::filesystem::path outputPath = "../output_data";
                outputPath /= newFilename;

                std::filesystem::create_directories(outputPath.parent_path());
                
                CsvWriter::WriteToFile(
                    outputPath.string(),
                    ServiceManager::getAggregator()->getColumnOrder(),
                    loadedData
                );
            }

            ServiceManager::getPlaybackManager()->setData(std::move(loadedData));
            transitionToConnectedState(AppState::CONNECTED_PLAYBACK);

        } else {
            ImGui::Begin("Caricamento in corso.");
            ImGui::Text("Analisi del file di telemetria. Attendere prego.");
            ImGui::End();
        }
    }

    for (auto& element : m_uiElements) {
        element->draw();
    }
    
    if (m_currentState == AppState::CONNECTED_LIVE || m_currentState == AppState::CONNECTED_PLAYBACK) {
        if (m_dashboard) m_dashboard->draw();
        if (m_accBrkWindow) m_accBrkWindow->draw();
        if (m_statusWindow) m_statusWindow->draw();
        if (m_steerWindow) m_steerWindow->draw();
        if (m_suspensionWindow) m_suspensionWindow->draw();
        if (m_hallWindow) m_hallWindow->draw();
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
    ImGui::Begin("DockSpaceHost", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (ImGui::BeginMainMenuBar()) {
        
        if (m_currentState != AppState::CONFIGURING) {
            if (ImGui::MenuItem("Home")) {
                resetToHome();
            }
        }

        if (ImGui::BeginMenu("Opzioni")) {
            if (ImGui::BeginMenu("Aspetto")) {
                bool isDark = ServiceManager::getSettingsManager()->getDarkMode();
                if (ImGui::MenuItem(isDark ? "Tema Chiaro" : "Tema Scuro")) {
                    ServiceManager::getSettingsManager()->setDarkMode(!isDark);
                    ApplyTheme(ImGui::GetStyle(), !isDark);
                }
                ImGui::Separator();
                ImGui::Text("Dimensione Testo");
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
    std::cout << "INFO [UiManager]: Reset dell'interfaccia verso la schermata Home." << std::endl;

    ServiceManager::resetForNewSession();

    m_dashboard.reset();
    m_accBrkWindow.reset();
    m_statusWindow.reset();
    m_steerWindow.reset();
    m_suspensionWindow.reset();
    m_hallWindow.reset();
    m_temperatureWindow.reset();
    m_playbackControls.reset();

    m_uiElements.clear();
    m_serialSelectionWindow = nullptr;
    m_fileSelectionWindow = nullptr;

    setupInitialState();
}


#if defined(WIN32)
void UiManager::SetWindowIconFromResource() {
    // ---- Funzione invariata ----
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

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<unsigned char> pixels(width * height * 4);
    GetDIBits(memDC, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    GLFWimage img{width, height, pixels.data()};
    glfwSetWindowIcon((GLFWwindow*)m_window, 1, &img);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);
}
#endif