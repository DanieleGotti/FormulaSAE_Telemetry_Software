#include "Telemetry/data_writing/DataAggregator.hpp"
#include <numeric>
#include <cmath>

DataAggregator::DataAggregator(std::function<void(const DbRow&)> onRowReady)
    : m_onRowReadyCallback(std::move(onRowReady)) {}

void DataAggregator::reset() {
    m_statsWindowStart = -1.0;
    m_statsBuffers.clear();
    m_currentStats.clear();
}

DataAggregator::StatData DataAggregator::calculateStats(const std::vector<double>& values) const {
    StatData stats;
    if (values.size() < 2) return stats; 
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    stats.mean = sum / values.size();
    double variance_sum = 0.0;
    for (double val : values) variance_sum += (val - stats.mean) * (val - stats.mean);
    stats.variance = variance_sum / (values.size() - 1); 
    stats.stdDev = std::sqrt(stats.variance);
    return stats;
}

void DataAggregator::processRow(DbRow row) {
    double timestamp = 0.0;
    try { timestamp = std::stod(row.at("timestamp")); } catch(...) {}

    if (m_statsWindowStart < 0.0) m_statsWindowStart = timestamp;

    // Raccoglie i dati nella finestra
    for (const auto& sensor : m_targetSensors) {
        if (row.count(sensor)) {
            try { m_statsBuffers[sensor].push_back(std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // Se sono passati 3 secondi, calcola e svuota
    if (timestamp - m_statsWindowStart >= STATS_WINDOW) {
        for (const auto& sensor : m_targetSensors) {
            m_currentStats[sensor] = calculateStats(m_statsBuffers[sensor]);
            m_statsBuffers[sensor].clear();
        }
        m_statsWindowStart = timestamp;
    }

    // Aggiunge le statistiche calcolate alla riga
    for (const auto& sensor : m_targetSensors) {
        row[sensor + "_mean"] = std::to_string(m_currentStats[sensor].mean);
        row[sensor + "_var"] = std::to_string(m_currentStats[sensor].variance);
        row[sensor + "_std"] = std::to_string(m_currentStats[sensor].stdDev);
    }

    if (m_onRowReadyCallback) m_onRowReadyCallback(row);
}