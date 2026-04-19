#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <mutex>

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
    std::replace(filePath.begin(), filePath.end(), '\\', '/');

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERRORE [FileService]: Impossibile aprire il file." << std::endl;
        m_isLoading = false;
        m_isFinished = true; 
        return;
    }

    std::string line;
    std::getline(file, line); // Salta l'header

    PacketParser parser;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        
        DbRow row;
        if (parser.parseCsvLine(line, row)) {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_loadedData.push_back(row);
        }
    }

    std::cout << "INFO[FileService]: Caricamento completato. " << m_loadedData.size() << " righe lette." << std::endl;
    m_isFinished = true;
    m_isLoading = false;
}