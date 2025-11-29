#include <fstream>
#include <iostream>
#include <mutex>
#include <algorithm>
#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include "Telemetry/data_writing/DataAggregator.hpp"
#include "Telemetry/Services/DataAggregatorService.hpp"

FileService::FileService() = default;

FileService::~FileService() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool FileService::configure(const FileConfig& config) {
    if (m_isLoading) return false;
    m_config = config;
    return true;
}

void FileService::startLoading() {
    if (m_isLoading || m_isFinished) return;
    m_isLoading = true;
    m_thread = std::thread(&FileService::loadingLoop, this);
}

bool FileService::isLoading() const { return m_isLoading; }
bool FileService::isFinished() const { return m_isFinished; }

std::vector<DbRow> FileService::getResult() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return std::move(m_loadedData);
}

void FileService::loadingLoop() {
    std::string filePath = m_config.filePath;
    // Normalizza i separatori di percorso per la massima compatibilità
    std::replace(filePath.begin(), filePath.end(), '\\', '/');

    std::cout << "INFO [FileService]: Tentativo di apertura del file: '" << filePath << "'." << std::endl;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERRORE [FileService]: Impossibile aprire il file. Controllare che il percorso sia corretto e che il file non sia usato da un altro programma." << std::endl;
        m_isLoading = false;
        m_isFinished = true; 
        return;
    }

    std::cout << "INFO [FileService]: File aperto con successo. Inizio caricamento." << std::endl;

    auto aggregatorConfig = DataAggregatorService::getDefaultConfig();

    DataAggregator aggregator(aggregatorConfig, [this](const DbRow& row){
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_loadedData.push_back(row);
    });

    std::string line;
    std::getline(file, line); 

    while (std::getline(file, line)) {
        // Rimuove il carattere di a capo '\r' se presente (tipico dei file Windows)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) continue;
        
        PacketParser packet = PacketParser::parse(line, {}, true);
        
        if (packet.packetType != PacketType::UNKNOWN) {
            aggregator.processPacket(packet);
        } else {
             std::cerr << "ATTENZIONE [FileService]: Riga scartata dal parser: '" << line << "'." << std::endl;
        }
    }

    aggregator.flush();

    std::cout << "INFO [FileService]: Caricamento del file completato. " << m_loadedData.size() << " righe aggregate." << std::endl;
    m_isFinished = true;
    m_isLoading = false;
}