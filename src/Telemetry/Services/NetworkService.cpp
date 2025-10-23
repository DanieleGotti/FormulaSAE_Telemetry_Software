#include <iostream>
#include "Telemetry/Services/NetworkService.hpp"

// DA IMPLEMENTARE

bool NetworkService::configure(const NetworkConfig& config) {
    return true;
}

bool NetworkService::start() {
    return true;
}

void NetworkService::stop() {
}

bool NetworkService::isRunning() const {
    return false;
}