#include <UI/UIManager.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>

UiManager::UiManager() {
        // Init GLFW
    if (!glfwInit())
        return;

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Necessario su macOS
#endif

    m_window = glfwCreateWindow(1280, 720, "Telemetry Dashboard", NULL, NULL);
    glfwMakeContextCurrent((GLFWwindow*)m_window);
    glfwSwapInterval(1); // vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Abilita Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // (opzionale) Abilita finestre multiple flottanti
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_window, true);

    ImGui_ImplOpenGL3_Init(glsl_version);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);
}

void UiManager::startMainLoop() {
    // Main loop
    while (!glfwWindowShouldClose((GLFWwindow*)m_window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        showDockingSpace();
        draw();

        // draw default ImGui demo window (for reference)
        ImGui::ShowDemoWindow(); // Show demo window! :)

        // === RENDER ===
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize((GLFWwindow*)m_window, &display_w, &display_h);
        // glViewport(0, 0, display_w, display_h);
        // glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // === FIX per Viewports ===
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }


        glfwSwapBuffers((GLFWwindow*)m_window);
    }
}

void UiManager::draw() {
    for (auto& element : m_uiElements) {
        element->draw();
    }
}

UiManager::~UiManager() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow((GLFWwindow*)m_window);
    glfwTerminate();
}

void UiManager::addElement(std::unique_ptr<UIElement> uiElement) {
    m_uiElements.push_back(std::move(uiElement));
}

void UiManager::removeElement(UIElement* uiElement) {
    m_uiElements.erase(std::remove_if(m_uiElements.begin(), m_uiElements.end(),
        [uiElement](const std::unique_ptr<UIElement>& elem) {
            return elem.get() == uiElement;
        }),
        m_uiElements.end());
}

void UiManager::showDockingSpace() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    // Rimuove titolo, bordi, background
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground;
    // window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar;
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));   // colore background finestra
    // ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(1.0f, 0.0f, 0.0f, 1.0f));   // colore bordo rosso

    ImGui::Begin("DockSpaceHost", nullptr, window_flags);

    ImGui::PopStyleVar(3);

    // DockSpace vero e proprio
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));


    // QUI il menù principale
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Esci")) {
                // Azione uscita
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Connessione seriale")) {
                // Toggle finestra
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::End();
    // ImGui::PopStyleColor(2);
}