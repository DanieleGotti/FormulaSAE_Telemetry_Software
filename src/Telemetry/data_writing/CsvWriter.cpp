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

bool CsvWriter::createFile(const std::string& directoryPath) {
    closeFile();

    std::string fileName = generate_csv_filename();
    std::filesystem::path fullPath = std::filesystem::path(directoryPath) / fileName;

    m_outputFile.open(fullPath, std::ios::out | std::ios::trunc);
    if (!m_outputFile.is_open()) {
        std::cerr << "ERROR [CsvWriter]: Impossible to create file: " << fullPath << std::endl;
        return false;
    }

    // Configurazione colonne database
    std::vector<ColumnConfig> config = {
        {"ACC1A", AggregationType::AVERAGE},
        {"ACC1B", AggregationType::AVERAGE},
        {"ACC2A", AggregationType::AVERAGE},
        {"ACC2B", AggregationType::AVERAGE},
        {"BRK1", AggregationType::AVERAGE},
        {"BRK2", AggregationType::AVERAGE},
        {"STEER", AggregationType::AVERAGE},
        {"LEFT_INVERTER_FSM", AggregationType::INVERTER},
        {"RIGHT_INVERTER_FSM", AggregationType::INVERTER},
        {"R2D_BUTTON", AggregationType::LAST},
        {"RESET_BUTTON", AggregationType::LAST},
        {"SDC_INPUT", AggregationType::LAST},
        {"TS_ON_BUTTON", AggregationType::LAST}
    };
    
    m_aggregator = std::make_unique<DataAggregator>(config, 
        [this](const DbRow& row){ this->onRowReady(row); }
    );
    
    // Scrive l'header del CSV
    m_columnOrder.push_back("timestamp");
    m_outputFile << "timestamp";
    for(const auto& col : config) {
        m_columnOrder.push_back(col.name);
        m_outputFile << ";" << col.name;
    }
    m_outputFile << std::endl;

    std::cout << "INFO [CsvWriter]: Recording started on file: " << fullPath << std::endl;
    return true;
}

void CsvWriter::onDataReceived(const PacketParser& packet) {
    if (m_aggregator) {
        m_aggregator->processPacket(packet);
    }
}

void CsvWriter::onRowReady(const DbRow& row) {
    if (!m_outputFile.is_open()) return;

    std::stringstream ss;
    for (size_t i = 0; i < m_columnOrder.size(); ++i) {
        const std::string& col_name = m_columnOrder[i];
        ss << row.at(col_name) << (i == m_columnOrder.size() - 1 ? "" : ";");
    }
    m_outputFile << ss.str() << std::endl;
}

void CsvWriter::stop() {
    if (m_aggregator) {
        m_aggregator->flush(); 
    }
    closeFile();
}

void CsvWriter::closeFile() {
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
}

