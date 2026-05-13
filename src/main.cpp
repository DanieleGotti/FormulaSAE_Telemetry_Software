#include <iostream>
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

int main() {

    std::cout << "INFO [Main]: Starting up." << std::endl;    
    ServiceManager::initialize();

    {
        UiManager uiManager;
        uiManager.run();
    } 
    
    ServiceManager::cleanup();
    std::cout << "INFO [Main]: Shutting down." << std::endl;

    return 0;
}