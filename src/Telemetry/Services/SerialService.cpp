#include <iostream>
#include <chrono>
#include "Telemetry/Services/SerialService.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp"

SerialService::SerialService(): m_dataSource(std::make_unique<SerialDataSource>()) {}

SerialService::~SerialService() {
    stop();
}

bool SerialService::configure(const SerialConfig& config) {
    if (m_running) {
        std::cerr << "Cannot configure SerialService while it is running." << std::endl;
        return false;
    }
    m_config = config;
    return true;
}

bool SerialService::start() {
    if (m_running) {
        std::cerr << "SerialService is already running." << std::endl;
        return true;
    }

    if (!m_dataSource->open(m_config.port, m_config.baudrate)) {
        std::cerr << "Failed to open data source for SerialService." << std::endl;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&SerialService::acquisitionLoop, this);
    std::cout << "SerialService started on port " << m_config.port << std::endl;
    return true;
}

void SerialService::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    
    m_dataSource->close();
    std::cout << "SerialService stopped." << std::endl;
}

bool SerialService::isRunning() const {
    return m_running;
}

void SerialService::acquisitionLoop() {
    while (m_running) {
        std::vector<uint8_t> packet = m_dataSource->readPacket();
        if (!packet.empty()) {
            // QUI PacketParser
            std::cout << "SerialService: Received " << packet.size() << " bytes." << std::endl;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}