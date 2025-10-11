#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <sstream>

#include "Telemetry/Services/ServiceManager.hpp"

#include "UI/UIManager.hpp"
#include "UI/SerialDeviceSelection.hpp"

int main() {
    ServiceManager::initialize();
    
    UiManager ui;
    ui.addElement(std::make_unique<SerialDeviceSelection>());
    ui.startMainLoop();

    return 0;
}