#include <iostream>
#include "Telemetry/Services/NetworkService.hpp"

bool NetworkService::configure(const NetworkConfig& config) {
    std::cout << "NetworkService configured with IP: " << config.ipAddress << " Port: " << config.port << std::endl;
    return true;
}

bool NetworkService::start() {
    std::cout << "NetworkService started." << std::endl;
    return true;
}

void NetworkService::stop() {
    std::cout << "NetworkService stopped." << std::endl;
}

bool NetworkService::isRunning() const {
    // Implementazione 
    return false;
}