#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "Telemetry/data_writing/DataAggregator.hpp"

// Helper function for formatting doubles with a fixed number of decimal places
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
    m_currentAgg.clear();
    m_sessionAgg.clear();
}

void DataAggregator::accumulate(const std::string& key, double value) {
    RunningStats& stats = m_currentAgg[key];
    if (value > stats.max) stats.max = value;
    stats.sum += value;
    stats.count++;
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

    // Reset if we receive a timestamp that is before the current window start 
    if (current_ts < m_statsWindowStart) {
        reset();
        m_statsWindowStart = current_ts;
    }

    // Add data to the current buffer
    for (const auto& sensor : m_targetSensors) {
        if (row.count(sensor)) {
            try { m_currentBuffers[sensor].push_back(std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // Speed is derived from the two rear wheels, the other channels are read as-is
    if (row.count("rear_left_velocity") && row.count("rear_right_velocity")) {
        try {
            accumulate("speed", (std::stod(row.at("rear_left_velocity")) + std::stod(row.at("rear_right_velocity"))) * 0.5);
        } catch (...) {}
    }
    for (const auto& sensor : m_maxMeanSensors) {
        if (row.count(sensor)) {
            try { accumulate(sensor, std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // If exactly 3 seconds have passed (tumbling window)
    if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
        for (const auto& sensor : m_targetSensors) {
            m_lastPublishedStats[sensor] = calculateStats(m_currentBuffers[sensor]);
            m_currentBuffers[sensor].clear(); // Clear the buffer for the next 3 seconds
        }

        for (const auto& entry : m_currentAgg) {
            RunningStats& session = m_sessionAgg[entry.first];
            if (entry.second.max > session.max) session.max = entry.second.max;
            session.sum += entry.second.sum;
            session.count += entry.second.count;
        }
        m_currentAgg.clear();

        // Avoid temporal drift, add exactly 3 seconds
        m_statsWindowStart += STATS_WINDOW; 
        
        // If there was a large jump in the data (e.g., missing signal for 10 seconds), realign it
        if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
            m_statsWindowStart = current_ts;
        }
    }

    // Write the formatted data in the row
    for (const auto& sensor : m_targetSensors) {
        if (m_lastPublishedStats.count(sensor)) {
            row[sensor + "_mean"] = formatDouble(m_lastPublishedStats[sensor].mean, 3);
            row[sensor + "_var"]  = formatDouble(m_lastPublishedStats[sensor].variance, 3);
            row[sensor + "_std"]  = formatDouble(m_lastPublishedStats[sensor].stdDev, 3);
        } else {
            // If we haven't passed the first 3 seconds since startup
            row[sensor + "_mean"] = "0.000";
            row[sensor + "_var"]  = "0.000";
            row[sensor + "_std"]  = "0.000";
        }
    }

    auto writeMaxMean = [&](const std::string& key) {
        const auto it = m_sessionAgg.find(key);
        if (it != m_sessionAgg.end() && it->second.count > 0) {
            row[key + "_max"]  = formatDouble(it->second.max, 3);
            row[key + "_mean"] = formatDouble(it->second.sum / it->second.count, 3);
        } else {
            // No window has been closed yet (first 3 seconds since startup)
            row[key + "_max"]  = "0.000";
            row[key + "_mean"] = "0.000";
        }
    };
    writeMaxMean("speed");
    for (const auto& sensor : m_maxMeanSensors) writeMaxMean(sensor);

    if (m_onRowReadyCallback) m_onRowReadyCallback(row);
}