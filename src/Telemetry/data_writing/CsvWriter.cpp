#include <chrono>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <sstream>
#include "Telemetry/data_writing/CsvWriter.hpp"

static std::string generate_csv_filename() {
    const auto now = std::chrono::system_clock::now();
    const auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return "telemetry_" + ss.str() + ".csv";
}

CsvWriter::CsvWriter() = default;

CsvWriter::~CsvWriter() {
    closeFile();
}

bool CsvWriter::createFile(const std::string& directoryPath, const std::vector<std::string>& columnOrder) {
    closeFile();
    m_columnOrder = columnOrder;
    m_isFirstRow = true;
    m_recordingStartTime = -1.0;

    std::string fileName = generate_csv_filename();
    std::filesystem::path fullPath = std::filesystem::path(directoryPath) / fileName;
    
    m_outputFile.open(fullPath, std::ios::out | std::ios::trunc);
    if (!m_outputFile.is_open()) {
        std::cerr << "ERRORE [CsvWriter]: Impossibile aprire il file per la registrazione." << std::endl;
        return false;
    }
    
    // Scrive l'header
    for (size_t i = 0; i < m_columnOrder.size(); ++i) {
        m_outputFile << m_columnOrder[i] << (i == m_columnOrder.size() - 1 ? "" : ";");
    }
    m_outputFile << std::endl;

    std::cout << "INFO [CsvWriter]: Registrazione avviata sul file: " << fullPath.string() << "." << std::endl;
    return true;
}

void CsvWriter::onAggregatedDataReceived(const DbRow& dataRow) {
    if (!m_outputFile.is_open()) return;

    double relative_ts = 0.0;
    bool has_ts = dataRow.count("timestamp");
    if (has_ts) {
        try {
            double current_ts = std::stod(dataRow.at("timestamp"));
            if (m_recordingStartTime < 0.0) {
                m_recordingStartTime = current_ts; // Salva il primo istante
            }
            relative_ts = current_ts - m_recordingStartTime; // Sottrae l'offset
        } catch(...) {}
    }

    std::stringstream ss;
    for (size_t i = 0; i < m_columnOrder.size(); ++i) {
        const std::string& col_name = m_columnOrder[i];
        
        // Se è la colonna timestamp formatta e scrivi il valore ricalcolato (relativo)
        if (col_name == "timestamp" && has_ts) {
            ss << std::fixed << std::setprecision(3) << relative_ts;
        } 
        // Altrimenti, per tutti gli altri valori, controlla la mappa e scrivi come prima
        else if (dataRow.count(col_name)) {
            ss << dataRow.at(col_name);
        }
        
        ss << (i == m_columnOrder.size() - 1 ? "" : ";");
    }
    m_outputFile << ss.str() << '\n';
}

void CsvWriter::stop() { 
    closeFile(); 
}

void CsvWriter::closeFile() { 
    if (m_outputFile.is_open()) m_outputFile.close(); 
}

bool CsvWriter::WriteToFile(const std::string& filePath, const std::vector<std::string>& columnOrder, const std::vector<DbRow>& data) {
    std::ofstream outputFile(filePath, std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        std::cerr << "ERRORE [CsvWriter]: Impossibile creare il file CSV: " << filePath << "." << std::endl;
        return false;
    }

    // Scrive l'header
    for (size_t i = 0; i < columnOrder.size(); ++i) {
        outputFile << columnOrder[i] << (i == columnOrder.size() - 1 ? "" : ";");
    }
    outputFile << '\n';

    // Scrive tutte le righe di dati
    for (const auto& dataRow : data) {
        for (size_t i = 0; i < columnOrder.size(); ++i) {
            const std::string& col_name = columnOrder[i];
            
            // Qui c'era già il controllo di sicurezza :)
            if (dataRow.count(col_name)) {
                outputFile << dataRow.at(col_name);
            }
            
            outputFile << (i == columnOrder.size() - 1 ? "" : ";");
        }
        outputFile << '\n';
    }

    std::cout << "INFO [CsvWriter]: File " << filePath << " generato con successo (" << data.size() << " righe)." << std::endl;
    return true;
}