#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <implot.h>
#include <utils/resources.hpp>
#include "UI/UIManager.hpp"
#include "UI/Theme.hpp"
#include "UI/SerialDeviceSelection.hpp"
#include "UI/LogTerminal.hpp"
#include "UI/AccBrkWindow.hpp"
#include "UI/StatusWindow.hpp"
#include "UI/SteerWindow.hpp"
#include "UI/SuspensionWindow.hpp"
#include "UI/HallWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

#ifdef WIN32
#include "../assets/resources.h" // Necessario per IDR_FONT_REGULAR/BOLD
#include <windows.h>
#endif

UiManager::UiManager() {
    if (!glfwInit())
        return;

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(1280, 720, "ERB - Telemetria", NULL, NULL);
    glfwMakeContextCurrent((GLFWwindow*)m_window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::cout << "INFO [UIManager]: Caricamento dei font." << std::endl;
    io.Fonts->Clear(); 

#ifdef WIN32
    SetWindowIconFromResource();
#endif

#ifdef __APPLE__
    // Su macOS, i font sono nel bundle, nella sottocartella 'fonts' di Resources
    font_body = io.Fonts->AddFontFromFileTTF(resources::getResourcePath("fonts/RobotoCondensed-Regular.ttf").c_str(), ServiceManager::getSettingsManager()->getBodyFontSize());
    font_label = io.Fonts->AddFontFromFileTTF(resources::getResourcePath("fonts/RobotoCondensed-Bold.ttf").c_str(), ServiceManager::getSettingsManager()->getLabelFontSize());
    font_data = io.Fonts->AddFontFromFileTTF(resources::getResourcePath("fonts/RobotoCondensed-Regular.ttf").c_str(), ServiceManager::getSettingsManager()->getDataFontSize());
    font_title = io.Fonts->AddFontFromFileTTF(resources::getResourcePath("fonts/RobotoCondensed-Bold.ttf").c_str(), ServiceManager::getSettingsManager()->getTitleFontSize());
#endif
#ifdef _WIN32
    // Carica FONT REGULAR 
    auto [pFontDataRegular, dFontSizeRegular] = resources::GetFontData(MAKEINTRESOURCE(IDR_FONT_REGULAR));
    if (pFontDataRegular && dFontSizeRegular > 0) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false; 

        font_body = io.Fonts->AddFontFromMemoryTTF(
            pFontDataRegular,
            dFontSizeRegular,
            ServiceManager::getSettingsManager()->getBodyFontSize(),
            &cfg
        );

        font_data = io.Fonts->AddFontFromMemoryTTF(
            pFontDataRegular,
            dFontSizeRegular,
            ServiceManager::getSettingsManager()->getDataFontSize(),
            &cfg
        );
    } else {
        io.Fonts->AddFontDefault(); 
    }

    // Carica FONT BOLD 
    auto [pFontDataBold, dFontSizeBold] = resources::GetFontData(MAKEINTRESOURCE(IDR_FONT_BOLD));
    if (pFontDataBold && dFontSizeBold > 0) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false; 

        font_label = io.Fonts->AddFontFromMemoryTTF(
            pFontDataBold,
            dFontSizeBold,
            ServiceManager::getSettingsManager()->getLabelFontSize(),
            &cfg
        );

        font_title = io.Fonts->AddFontFromMemoryTTF(
            pFontDataBold,
            dFontSizeBold,
            ServiceManager::getSettingsManager()->getTitleFontSize(),
            &cfg
        );
    } else {
        io.Fonts->AddFontDefault(); 
    }
#endif


    ImGui::GetIO().FontGlobalScale = ServiceManager::getSettingsManager()->getGlobalFontScale();
    assert(font_body != nullptr && "ERRORE: Impossibile caricare i font. Controlla i percorsi."); 
    std::cout << "INFO [UIManager]: Font caricati." << std::endl;    
    
    ApplyTheme(ImGui::GetStyle(), ServiceManager::getSettingsManager()->getDarkMode());
    
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    addElement(std::make_unique<LogTerminal>(this));    
    setupInitialState();
}

UiManager::~UiManager() {
    // Rimuove la sottoscrizione della finestre per evitare puntatori dangling
    if (ServiceManager::getAggregator()) {
        if (m_dashboard) {
            ServiceManager::getAggregator()->unsubscribe(m_dashboard.get());
        }
        if (m_accBrkWindow) { 
            ServiceManager::getAggregator()->unsubscribe(m_accBrkWindow.get());
        }
        if (m_statusWindow) { 
            ServiceManager::getAggregator()->unsubscribe(m_statusWindow.get());
        }
        if (m_steerWindow) { 
            ServiceManager::getAggregator()->unsubscribe(m_steerWindow.get());
        }
        if (m_suspensionWindow) {
            ServiceManager::getAggregator()->unsubscribe(m_suspensionWindow.get());
        }
        if (m_hallWindow) {
            ServiceManager::getAggregator()->unsubscribe(m_hallWindow.get());
        }
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow((GLFWwindow*)m_window);
    glfwTerminate();
}

void UiManager::setupInitialState() {
    auto onConnect = [this](const std::string& port, int baudrate) {
        if (ServiceManager::configureSerial(port, baudrate) && ServiceManager::startServices()) {
           
            // Passo un puntatore a UiManager per accedere ai font
            this->m_dashboard = std::make_shared<Dashboard>(this);
            ServiceManager::getAggregator()->subscribe(this->m_dashboard.get());
            
            this->m_accBrkWindow = std::make_shared<AccBrkWindow>(this); 
            ServiceManager::getAggregator()->subscribe(this->m_accBrkWindow.get());
            
            this->m_statusWindow = std::make_shared<StatusWindow>(this);
            ServiceManager::getAggregator()->subscribe(this->m_statusWindow.get());

            this->m_steerWindow = std::make_shared<SteerWindow>(this); 
            ServiceManager::getAggregator()->subscribe(this->m_steerWindow.get());

            this->m_suspensionWindow = std::make_shared<SuspensionWindow>(this);    
            ServiceManager::getAggregator()->subscribe(this->m_suspensionWindow.get()); 

            this->m_hallWindow = std::make_shared<HallWindow>(this);
            ServiceManager::getAggregator()->subscribe(this->m_hallWindow.get());

            if (m_serialSelectionWindow) {
                this->removeElement(m_serialSelectionWindow);
                m_serialSelectionWindow = nullptr;
            }
            this->m_currentState = AppState::CONNECTED;
        } else {
            std::cerr << "ERRORE [UIManager]: Connessione fallita." << std::endl;
        }
    };
    
    auto selectionWindow = std::make_unique<SerialDeviceSelection>(this, onConnect);
    m_serialSelectionWindow = selectionWindow.get();
    addElement(std::move(selectionWindow));
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
        
        //ImGui::ShowDemoWindow();
        
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
    for (auto& element : m_uiElements) {
        element->draw();
    }
    // Disegna le finestre se sono state create
    if (m_dashboard) {
        m_dashboard->draw();
    }
    if (m_accBrkWindow) { 
        m_accBrkWindow->draw();
    }
    if (m_statusWindow) {
        m_statusWindow->draw();
    }
    if (m_steerWindow) {
        m_steerWindow->draw();
    }
    if (m_suspensionWindow) {
        m_suspensionWindow->draw();
    }
    if (m_hallWindow) {
        m_hallWindow->draw();
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
                [elementToRemove](const std::unique_ptr<UIElement>& elem) {
                    return elem.get() == elementToRemove;
                }),
            m_uiElements.end());
    }
    m_elementsToRemove.clear();
}

void UiManager::showDockingSpace() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground; 

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
        if (ImGui::BeginMenu("Opzioni")) {
          if (ImGui::BeginMenu("Aspetto")) {
                bool changed = false;
                float darkMode = ServiceManager::getSettingsManager()->getDarkMode();
                const char* themeLabel = darkMode ? "Tema chiaro" : "Tema scuro";
                if (ImGui::MenuItem(themeLabel)) {
                    ServiceManager::getSettingsManager()->setDarkMode(!darkMode);
                    ApplyTheme(ImGui::GetStyle(), ServiceManager::getSettingsManager()->getDarkMode());
                }

                ImGui::Separator();

                // Controllo dimensione font
                ImGui::Text("Dimensione testo");
                ImGui::SameLine();
                float fontGlobalScale = ServiceManager::getSettingsManager()->getGlobalFontScale();

                if (ImGui::Button("-")) {
                    ServiceManager::getSettingsManager()->setGlobalFontScale((std::max)(0.5f, fontGlobalScale - 0.1f));
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("+")) {
                    ServiceManager::getSettingsManager()->setGlobalFontScale((std::max)(0.5f, fontGlobalScale + 0.1f));
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::Text("%.1fx", fontGlobalScale);

                if(changed) {
                    ImGuiIO& io = ImGui::GetIO();
                    io.FontGlobalScale = ServiceManager::getSettingsManager()->getGlobalFontScale();

                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::End();
}

// void UiManager::toggleTheme() {
//     m_isDarkTheme = !m_isDarkTheme; 
//     if (m_isDarkTheme) {
//         ImGui::StyleColorsDark();
//     } else {
//         ImGui::StyleColorsLight();
//     }
//
//     ImGuiStyle& style = ImGui::GetStyle();
//     if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
//         style.WindowRounding = 0.0f;
//         style.Colors[ImGuiCol_WindowBg].w = 1.0f;
//     }
// }

#if defined(WIN32)
void UiManager::SetWindowIconFromResource()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    // Carica l'icona dal file eseguibile (risorsa IDI_APP_ICON)
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_ICON1));

    // Converte l'HICON in formato GLFWimage
    ICONINFO iconInfo;
    BITMAP bmpColor;
    GetIconInfo(hIcon, &iconInfo);
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmpColor);

    int width = bmpColor.bmWidth;
    int height = bmpColor.bmHeight;

    // Copia i pixel in memoria (BGRA)
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

    // Crea l’immagine GLFW
    GLFWimage img{};
    img.width = width;
    img.height = height;
    img.pixels = pixels.data();
    glfwSetWindowIcon((GLFWwindow*)m_window, 1, &img);

    // Cleanup
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);
}
#endif