#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include "Telemetry/data_writing/CsvWriter.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

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

    std::string ext = std::filesystem::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".csv") {
        loadCsv(filePath);
    } else if (ext == ".bin") {
        loadBin(filePath);
    } else {
        std::cerr << "ERRORE[FileService]: Formato file non supportato (" << ext << ")." << std::endl;
        m_isLoading = false;
        m_isFinished = true;
    }
}

void FileService::loadCsv(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERRORE [FileService]: Impossibile aprire il file CSV." << std::endl;
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

    std::cout << "INFO[FileService]: Caricamento CSV completato. " << m_loadedData.size() << " righe lette." << std::endl;
    m_isFinished = true;
    m_isLoading = false;
}

void FileService::loadBin(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERRORE [FileService]: Impossibile aprire il file BIN." << std::endl;
        m_isLoading = false;
        m_isFinished = true;
        return;
    }

    PacketParser parser;
    std::vector<DbRow> rawRows;
    std::vector<uint8_t> buffer;
    
    // Legge il file a blocchi da 8KB
    const size_t CHUNK_SIZE = 8192;
    uint8_t temp[CHUNK_SIZE];

    while (file.read(reinterpret_cast<char*>(temp), CHUNK_SIZE) || file.gcount() > 0) {
        size_t count = file.gcount();
        buffer.insert(buffer.end(), temp, temp + count);
        
        // Passa i byte al parser esattamente come nella modalità LIVE
        DbRow row;
        while (parser.parseLiveBytes(buffer, row)) {
            rawRows.push_back(row);
        }
    }

    std::cout << "INFO [FileService]: BIN decodificato in " << rawRows.size() << " pacchetti. Calcolo packet lost e varianze in corso..." << std::endl;

    std::vector<DbRow> aggregatedRows;
    aggregatedRows.reserve(rawRows.size());

    // Ricicliamo la classe DataAggregator per calcolare tutto offline!
    DataAggregator aggregator([&aggregatedRows](const DbRow& r) {
        aggregatedRows.push_back(r);
    });

    for (const auto& r : rawRows) {
        aggregator.processRow(r);
    }

    // Genera il nome del file CSV in automatico di fianco al .bin
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    std::string outName = "telemetry_from_bin_" + ss.str() + ".csv";
    
    std::filesystem::path outPath = std::filesystem::path(filePath).parent_path() / outName;

    // Salva il CSV
    CsvWriter::WriteToFile(outPath.string(), PacketParser::getColumnOrder(), aggregatedRows);

    // Carica il risultato nella UI per fare il playback
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_loadedData = std::move(aggregatedRows);
    }

    std::cout << "INFO [FileService]: Conversione completata. File generato: " << outPath.string() << std::endl;
    m_isFinished = true;
    m_isLoading = false;
}