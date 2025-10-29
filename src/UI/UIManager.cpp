#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <implot.h>
#include "UI/UIManager.hpp"
#include "UI/Theme.hpp"
#include "UI/SerialDeviceSelection.hpp"
#include "UI/LogTerminal.hpp"
#include "UI/AccBrkWindow.hpp"
#include "UI/StatusWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

UiManager::UiManager() {
    if (!glfwInit())
        return;

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(1280, 720, "Telemetry ", NULL, NULL);
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
    font_body = io.Fonts->AddFontFromFileTTF("../external/fonts/RobotoCondensed-Regular.ttf", 20.0f);
    font_label = io.Fonts->AddFontFromFileTTF("../external/fonts/RobotoCondensed-Bold.ttf", 20.0f);
    font_data = io.Fonts->AddFontFromFileTTF("../external/fonts/RobotoCondensed-Regular.ttf", 22.0f);
    font_title = io.Fonts->AddFontFromFileTTF("../external/fonts/RobotoCondensed-Bold.ttf", 22.0f);
    assert(font_body != nullptr && "ERRORE: Impossibile caricare i font. Controlla i percorsi."); 
    std::cout << "INFO [UIManager]: Font caricati." << std::endl;    
    
    m_isDarkTheme = true;
    ApplyTheme(ImGui::GetStyle(), m_isDarkTheme);
    
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
                const char* themeLabel = m_isDarkTheme ? "Tema chiaro" : "Tema scuro";
                if (ImGui::MenuItem(themeLabel)) {
                    m_isDarkTheme = !m_isDarkTheme;
                    ApplyTheme(ImGui::GetStyle(), m_isDarkTheme);
                }

                ImGui::Separator();

                // Controllo dimensione font
                ImGui::Text("Dimensione testo");
                ImGui::SameLine();

                if (ImGui::Button("-")) {
                    ImGui::GetIO().FontGlobalScale = std::max(0.5f, ImGui::GetIO().FontGlobalScale - 0.1f);
                }
                ImGui::SameLine();
                if (ImGui::Button("+")) {
                    ImGui::GetIO().FontGlobalScale = std::min(2.0f, ImGui::GetIO().FontGlobalScale + 0.1f);
                }
                ImGui::SameLine();
                ImGui::Text("%.1fx", ImGui::GetIO().FontGlobalScale);

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::End();
}

void UiManager::toggleTheme() {
    m_isDarkTheme = !m_isDarkTheme; 
    if (m_isDarkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}