/**
 * @file DataAggregator.cpp
 * @brief Implementation of the live statistics computation (see DataAggregator.hpp).
 */
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "Telemetry/data_writing/DataAggregator.hpp"

/**
 * @brief Formats a double with a fixed number of decimals and a dot as separator.
 * @param val Value.
 * @param dec Number of decimals.
 * @return Formatted text ("C" locale, independent of the system settings).
 */
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
    m_distanceMeters = 0.0;
    m_lastDistanceTs = -1.0;
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

    // Timestamp earlier than the window start (e.g. ECU reset): start over
    if (current_ts < m_statsWindowStart) {
        reset();
        m_statsWindowStart = current_ts;
    }

    // Add the values to the current window
    for (const auto& sensor : m_targetSensors) {
        if (row.count(sensor)) {
            try { m_currentBuffers[sensor].push_back(std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // Speed is derived from the two rear wheels, the other channels are read as-is
    if (row.count("rear_left_velocity") && row.count("rear_right_velocity")) {
        try {
            const double speed_kmh = (std::stod(row.at("rear_left_velocity")) + std::stod(row.at("rear_right_velocity"))) * 0.5;
            accumulate("speed", speed_kmh);

            // Distance is the speed integrated over the elapsed time. A gap
            // longer than the threshold means samples were lost, not that the
            // car crawled, so that interval is not integrated.
            if (m_lastDistanceTs >= 0.0) {
                const double dt = current_ts - m_lastDistanceTs;
                if (dt > 0.0 && dt <= TelemetrySchema::MAX_DISTANCE_GAP_S) {
                    m_distanceMeters += speed_kmh * TelemetrySchema::KMH_TO_MS * dt;
                }
            }
            m_lastDistanceTs = current_ts;
        } catch (...) {}
    }
    for (const auto& sensor : m_maxMeanSensors) {
        // "speed" has no column of its own, it was accumulated just above
        if (sensor != "speed" && row.count(sensor)) {
            try { accumulate(sensor, std::stod(row.at(sensor))); } catch (...) {}
        }
    }

    // Close the window once 3 seconds have passed ("tumbling" window)
    if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
        for (const auto& sensor : m_targetSensors) {
            m_lastPublishedStats[sensor] = calculateStats(m_currentBuffers[sensor]);
            m_currentBuffers[sensor].clear(); // clear the buffer for the next 3 seconds
        }

        // Fold the window accumulators into the session totals
        for (const auto& entry : m_currentAgg) {
            RunningStats& session = m_sessionAgg[entry.first];
            if (entry.second.max > session.max) session.max = entry.second.max;
            session.sum += entry.second.sum;
            session.count += entry.second.count;
        }
        m_currentAgg.clear();

        // Advance by exactly 3 seconds to avoid temporal drift
        m_statsWindowStart += STATS_WINDOW; 
        
        // After a large jump in the data (e.g. signal missing for 10 s), realign
        if (current_ts - m_statsWindowStart >= STATS_WINDOW) {
            m_statsWindowStart = current_ts;
        }
    }

    // Write the formatted statistics into the row
    for (const auto& sensor : m_targetSensors) {
        if (m_lastPublishedStats.count(sensor)) {
            row[sensor + "_mean"] = formatDouble(m_lastPublishedStats[sensor].mean, 3);
            row[sensor + "_var"]  = formatDouble(m_lastPublishedStats[sensor].variance, 3);
            row[sensor + "_std"]  = formatDouble(m_lastPublishedStats[sensor].stdDev, 3);
        } else {
            // The first 3 seconds since startup have not passed yet
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
            // No window closed yet (first 3 seconds since startup)
            row[key + "_max"]  = "0.000";
            row[key + "_mean"] = "0.000";
        }
    };
    for (const auto& sensor : m_maxMeanSensors) writeMaxMean(sensor);

    row["meters"] = formatDouble(m_distanceMeters, 1);

    if (m_onRowReadyCallback) m_onRowReadyCallback(row);
}