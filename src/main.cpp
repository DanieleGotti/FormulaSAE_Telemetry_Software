#include <iostream>
#include "Telemetry/Services/ServiceManager.hpp"
#include "UI/UIManager.hpp"

int main() {
    
    ServiceManager::initialize();

    {
        UiManager uiManager;
        uiManager.run();
    } 
    
    ServiceManager::cleanup();
    std::cout << "Cleanup completed. Exiting." << std::endl;

    return 0;
}