#include <iostream>
#include "Telemetry/Services/ServiceManager.hpp"
#include "UI/UIManager.hpp"

int main() {

    std::cout << "INFO [Main]: Avvio in corso." << std::endl;    
    ServiceManager::initialize();

    {
        UiManager uiManager;
        uiManager.run();
    } 
    
    ServiceManager::cleanup();
    std::cout << "INFO [Main]: Chiusura in corso." << std::endl;

    return 0;
}