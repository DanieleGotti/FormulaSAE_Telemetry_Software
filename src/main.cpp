#include <iostream>
#include <memory>
#include <filesystem> 
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_writing/TxtWriter.hpp"
#include "UI/UIManager.hpp"
#include "UI/SerialDeviceSelection.hpp"
#include "UI/Dashboard.hpp"

// Stati dell'applicazione
enum class AppState {
    CONFIGURING, // Mostra la finestra di selezione della porta
    CONNECTED    // Mostra la dashboard
};

int main() {
    const std::string OUTPUT_DIRECTORY = "../output_data";
    ServiceManager::initialize();
    UiManager uiManager;
    AppState currentState = AppState::CONFIGURING;
    std::shared_ptr<TxtWriter> txtWriter;

    // Gestione della registrazione 
    auto onStopLogging = [&]() {
        if (!txtWriter) return;
        std::cout << "Starting stop logging procedure..." << std::endl;
        
        // Sgancia i subscribers
        ServiceManager::getDataManager()->removeSubscriber(txtWriter);
        std::cout << "INFO: TxtWriter rimosso dai subscriber." << std::endl;
        txtWriter.reset(); 
        std::cout << "INFO: Recording stopped and resources released." << std::endl;
    };

    auto onStartLogging = [&]() {
        if (txtWriter) return;
        std::filesystem::create_directories(OUTPUT_DIRECTORY);
        txtWriter = std::make_shared<TxtWriter>();
        if (txtWriter->createFile(OUTPUT_DIRECTORY)) {
             ServiceManager::getDataManager()->addSubscriber(txtWriter);
             txtWriter->start(); 
        } else {
             txtWriter.reset();
        }
    };

    auto dashboard = std::make_shared<Dashboard>(onStartLogging, onStopLogging);

    // Gestione della connessione
    auto onConnect = [&](const std::string& port, int baudrate) {
        if (ServiceManager::configureSerial(port, baudrate)) {
            ServiceManager::setAcquisitionMethod(ACQUISITION_METHOD_SERIAL);
            ServiceManager::getDataManager()->addSubscriber(dashboard);
            ServiceManager::startServices();
            currentState = AppState::CONNECTED;
        } else {
            std::cerr << "ERROR: Impossible to connect to port " << port << std::endl;
        }
    };

    SerialDeviceSelection selectionWindow(onConnect);

    while (!glfwWindowShouldClose((GLFWwindow*)uiManager.getWindowHandle())) {
        uiManager.beginFrame();

        switch (currentState) {
            case AppState::CONFIGURING:
                selectionWindow.draw();
                break;
            case AppState::CONNECTED:
                {
                    std::string currentFile = (txtWriter) ? txtWriter->getCurrentFileName() : "";
                    dashboard->setLoggingStatus(txtWriter != nullptr, currentFile);
                    dashboard->draw();
                }
                break;
        }
        ImGui::ShowDemoWindow();
        uiManager.endFrame();
    }

    // Sequenza di chiusura ordinata
    std::cout << "Closing application..." << std::endl;
    // Ferma la registrazione se attiva
    ServiceManager::stopTasks();
    // Drenaggio finale dei dati rimanenti
    if (txtWriter) {
        onStopLogging();
    }
    // Cleanup finale di tutti i servizi
    ServiceManager::cleanup();
    std::cout << "Cleanup completed. Exiting." << std::endl;
    return 0;
}