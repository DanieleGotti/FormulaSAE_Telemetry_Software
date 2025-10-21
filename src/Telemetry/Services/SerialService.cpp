#include <iostream>
#include <string>
#include <chrono>
#include "Telemetry/Services/SerialService.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

SerialService::SerialService(DataManager* dataManager): m_dataSource(std::make_unique<SerialDataSource>()), m_dataManager(dataManager) {}

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
    std::string serialBuffer;

    while (m_running) {
        std::vector<uint8_t> rawData = m_dataSource->readPacket();
        if (!rawData.empty()) {
            const auto reception_time = std::chrono::system_clock::now();
            serialBuffer.append(rawData.begin(), rawData.end());
            size_t pos;
            while ((pos = serialBuffer.find('\n')) != std::string::npos) {
                std::string line = serialBuffer.substr(0, pos);
                serialBuffer.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.empty()) continue;

                PacketParser packet = PacketParser::parse(line, reception_time);
                
                if (packet.packetType != PacketType::UNKNOWN && m_dataManager) {
                    m_dataManager->processData(packet);
                } else if (m_dataManager) { 
                    // Log dei pacchetti malformati per debug
                    std::cerr << "Raw data discarded by parser -> '" << std::get<std::string>(packet.data) << "'" << std::endl;
                }
            }
            std::cout << "SerialService: Received " << rawData.size() << " bytes." << std::endl;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}