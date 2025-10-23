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

    std::string fileName = generate_csv_filename();
    std::filesystem::path fullPath = std::filesystem::path(directoryPath) / fileName;
    m_outputFile.open(fullPath, std::ios::out | std::ios::trunc);
    if (!m_outputFile.is_open()) return false;
    
    // Scrive l'header
    for (size_t i = 0; i < m_columnOrder.size(); ++i) {
        m_outputFile << m_columnOrder[i] << (i == m_columnOrder.size() - 1 ? "" : ";");
    }
    m_outputFile << std::endl;

    std::cout << "INFO [CsvWriter]: Registrazione avviata sul file: " << fullPath << "." << std::endl;
    return true;
}

// Riceve la riga pronta
void CsvWriter::onAggregatedDataReceived(const DbRow& dataRow) {
    if (!m_outputFile.is_open()) return;

    std::stringstream ss;
    for (size_t i = 0; i < m_columnOrder.size(); ++i) {
        const std::string& col_name = m_columnOrder[i];
        ss << dataRow.at(col_name) << (i == m_columnOrder.size() - 1 ? "" : ";");
    }
    m_outputFile << ss.str() << std::endl;
}

void CsvWriter::stop() { 
    closeFile(); 
}

void CsvWriter::closeFile() { 
    if (m_outputFile.is_open()) m_outputFile.close(); 
}