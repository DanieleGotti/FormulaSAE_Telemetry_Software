#include <iostream>
#include <string>
#include <chrono>
#include "Telemetry/Services/SerialService.hpp"
#include "Telemetry/data_acquisition/SerialDataSource.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

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
    // Tengo i dati letti in un buffer per evitare di perdere dati tra una lettura e l'altra
    std::string serialBuffer;

    while (m_running) {
        std::vector<uint8_t> rawData = m_dataSource->readPacket();
        if (!rawData.empty()) {
            // Aggiungo dati letti
            serialBuffer.append(rawData.begin(), rawData.end());
            size_t pos;
            // Ciclo finchè non trovo un \n
            while ((pos = serialBuffer.find('\n')) != std::string::npos) {
                std::string line = serialBuffer.substr(0, pos);
                serialBuffer.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) continue;
                // Mando al PacketParser per il controllo e la decodifica
                PacketParser packet = PacketParser::parse(line);
                if (packet.packetType != PacketType::UNKNOWN) {
                    // CHIAMO DATA MANAGER

                } else {
                    std::cerr << "Unknown packet -> '" << std::get<std::string>(packet.data) << "'" << std::endl;
                }
            }
            std::cout << "SerialService: Received " << rawData.size() << " bytes." << std::endl;
        } else {
            // Pausa se non ci sono dati da leggere
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}