#include "Telemetry/data_writing/DataAggregator.hpp"
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

// Helper per la formattazione univoca a N decimali
static std::string formatDouble(double val, int dec) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic()); 
    ss << std::fixed << std::setprecision(dec) << val;
    return ss.str();
}

DataAggregator::DataAggregator(std::function<void(const DbRow&)> onRowReady)
    : m_onRowReadyCallback(std::move(onRowReady)) {}

void DataAggregator::reset() {
    m_statsWindowStart = -1.0;
    m_currentBuffers.clear();
    m_lastPublishedStats.clear();
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
    double current_ts = 0.0;
    try { current_ts = std::stod(row.at("timestamp")); } catch(...) { return; }

    if (m_statsWindowStart < 0.0) m_statsWindowStart = current_ts;

    // Gestione salti temporali all'indietro (reset)
    if (current_ts < m_statsWindowStart) {
        reset();
        m_statsWindowStart = current_ts;
    }

    // Accumula nel buffer corrente
    for (const auto& sensor : m_targetSensors) {
        if (row.count(sensor)) {
            try { m_currentBuffers[sensor].push_back(std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // Se sono passati i 3 secondi ESATTI (tumbling window)
    if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
        for (const auto& sensor : m_targetSensors) {
            m_lastPublishedStats[sensor] = calculateStats(m_currentBuffers[sensor]);
            m_currentBuffers[sensor].clear(); // Svuota il buffer per i prossimi 3 sec
        }
        
        // Evita derive temporali, aggiunge esattamente 3 secondi
        m_statsWindowStart += STATS_WINDOW; 
        
        // Se c'è stato un mega-salto nei dati (es. mancato segnale per 10 secondi), lo riallinea
        if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
            m_statsWindowStart = current_ts;
        }
    }

    // Scrive i dati formattati nella riga. 
    // Così per 2.99 secondi Dataset e UI leggono l'ultimo blocco valido, 
    // allo scoccare dei 3s la stringa si aggiorna con i nuovi calcoli.
    for (const auto& sensor : m_targetSensors) {
        if (m_lastPublishedStats.count(sensor)) {
            row[sensor + "_mean"] = formatDouble(m_lastPublishedStats[sensor].mean, 3);
            row[sensor + "_var"]  = formatDouble(m_lastPublishedStats[sensor].variance, 3);
            row[sensor + "_std"]  = formatDouble(m_lastPublishedStats[sensor].stdDev, 3);
        } else {
            // Se non sono ancora passati i primi 3 secondi dall'avvio
            row[sensor + "_mean"] = "0.000";
            row[sensor + "_var"]  = "0.000";
            row[sensor + "_std"]  = "0.000";
        }
    }

    if (m_onRowReadyCallback) m_onRowReadyCallback(row);
}