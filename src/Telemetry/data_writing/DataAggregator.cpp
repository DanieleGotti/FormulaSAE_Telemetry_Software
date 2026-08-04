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
    m_currentSpeed = SpeedStats{};
    m_sessionSpeed = SpeedStats{};
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

    if (row.count("rear_left_velocity") && row.count("rear_right_velocity")) {
        try {
            double speed = (std::stod(row.at("rear_left_velocity")) + std::stod(row.at("rear_right_velocity"))) * 0.5;
            if (speed > m_currentSpeed.max) m_currentSpeed.max = speed;
            m_currentSpeed.sum += speed;
            m_currentSpeed.count++;
        } catch (...) {}
    }

    // If exactly 3 seconds have passed (tumbling window)
    if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
        for (const auto& sensor : m_targetSensors) {
            m_lastPublishedStats[sensor] = calculateStats(m_currentBuffers[sensor]);
            m_currentBuffers[sensor].clear(); // Clear the buffer for the next 3 seconds
        }

        if (m_currentSpeed.max > m_sessionSpeed.max) m_sessionSpeed.max = m_currentSpeed.max;
        m_sessionSpeed.sum += m_currentSpeed.sum;
        m_sessionSpeed.count += m_currentSpeed.count;
        m_currentSpeed = SpeedStats{};

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

    row["speed_max"]  = formatDouble(m_sessionSpeed.max, 3);
    row["speed_mean"] = formatDouble(m_sessionSpeed.count > 0 ? m_sessionSpeed.sum / m_sessionSpeed.count : 0.0, 3);

    if (m_onRowReadyCallback) m_onRowReadyCallback(row);
}