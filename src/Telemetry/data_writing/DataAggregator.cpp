#include <sstream>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <cmath> 
#include <locale> 
#include "Telemetry/data_writing/DataAggregator.hpp"

// Converte un double in stringa con precisione fissa
static std::string formatDouble(double value, int precision) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

// Formatta le statistiche ignorando la lingua di sistema 
static std::string formatStat(double value) {
    std::stringstream ss;
    ss.imbue(std::locale::classic()); 
    ss << std::fixed << std::setprecision(3) << value;
    return ss.str();
}

DataAggregator::DataAggregator(std::vector<ColumnConfig> config, std::function<void(const DbRow&)> onRowReady)
    : m_config(std::move(config)), m_onRowReadyCallback(std::move(onRowReady)) {
    for (const auto& col : m_config) {
        m_configMap[col.name] = col.type;
    }
}

DataAggregator::StatData DataAggregator::calculateStats(const std::vector<double>& values) const {
    StatData stats;
    if (values.size() < 2) return stats; 
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    stats.mean = sum / values.size();
    double variance_sum = 0.0;
    for (double val : values) {
        variance_sum += (val - stats.mean) * (val - stats.mean);
    }
    stats.variance = variance_sum / (values.size() - 1); 
    stats.stdDev = std::sqrt(stats.variance);
    return stats;
}

double DataAggregator::getNumericValue(const PacketData& data) {
    if (std::holds_alternative<int>(data)) return static_cast<double>(std::get<int>(data));
    if (std::holds_alternative<double>(data)) return std::get<double>(data);
    return 0.0; 
}

void DataAggregator::processPacket(const PacketParser& packet) {
    if (m_currentTimestamp.time_since_epoch().count() == 0) {
        m_currentTimestamp = packet.timestamp;
    }

    if (packet.timestamp != m_currentTimestamp) {
        finalizeAndEmitRow();
        m_currentTimestamp = packet.timestamp;
    }

    auto it = m_configMap.find(packet.label);
    if (it != m_configMap.end()) {
        AggregationData& data = m_currentRowData[packet.label];
        data.lastValue = packet.data;
        data.numericValues.push_back(getNumericValue(packet.data));
        if (std::holds_alternative<std::string>(packet.data)) {
            data.stringValues.push_back(std::get<std::string>(packet.data));
        }
    }

    // Gestione delle statistiche per i sensori target
    if (std::find(m_targetSensors.begin(), m_targetSensors.end(), packet.label) != m_targetSensors.end()) {
        if (m_statsWindowStart.time_since_epoch().count() == 0) {
            m_statsWindowStart = packet.timestamp;
        }

        m_statsBuffers[packet.label].push_back(getNumericValue(packet.data));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(packet.timestamp - m_statsWindowStart).count();
        if (elapsed >= STATS_WINDOW) {
            for (const auto& sensor : m_targetSensors) {
                m_currentStats[sensor] = calculateStats(m_statsBuffers[sensor]);
                m_statsBuffers[sensor].clear();
            }
            m_statsWindowStart = packet.timestamp;
        }
    }
}

void DataAggregator::finalizeAndEmitRow() {
    if (m_currentRowData.empty() && m_currentTimestamp.time_since_epoch().count() == 0) {
        return; 
    }
    
    DbRow finalRow;
    
    auto in_time_t = std::chrono::system_clock::to_time_t(m_currentTimestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_currentTimestamp.time_since_epoch()) % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    finalRow["timestamp"] = ss.str();

    for (const auto& col_config : m_config) {
        const std::string& col_name = col_config.name;
        auto it = m_currentRowData.find(col_name);

        if (it != m_currentRowData.end()) {
            const AggregationData& data = it->second;
            std::string valToEmit;
            std::string valToLatch;

            switch (col_config.type) {
                case AggregationType::AVERAGE: {
                    double sum = std::accumulate(data.numericValues.begin(), data.numericValues.end(), 0.0);
                    double avg = data.numericValues.empty() ? 0.0 : sum / data.numericValues.size();
                    if (col_config.format == OutputFormat::INTEGER) valToEmit = std::to_string(static_cast<int>(std::round(avg)));
                    else valToEmit = formatDouble(avg, 1);
                    valToLatch = valToEmit;
                    break;
                }
                case AggregationType::LAST: {
                    double last_val = getNumericValue(data.lastValue);
                    if (col_config.format == OutputFormat::INTEGER) valToEmit = std::to_string(static_cast<int>(last_val));
                    else valToEmit = formatDouble(last_val, 1); 
                    valToLatch = valToEmit;
                    break;
                }
                case AggregationType::INVERTER: {
                    std::string combined_states;
                    for (size_t i = 0; i < data.stringValues.size(); ++i) {
                        combined_states += data.stringValues[i] + (i == data.stringValues.size() - 1 ? "" : " / ");
                    }
                    valToEmit = combined_states;
                    if (!data.stringValues.empty()) valToLatch = data.stringValues.back();
                    else {
                         if (m_lastKnownValues.count(col_name)) valToLatch = m_lastKnownValues[col_name];
                         else valToLatch = "UNKNOWN";
                    }
                    break;
                }
            }
            finalRow[col_name] = valToEmit;
            m_lastKnownValues[col_name] = valToLatch;
        } 
        else {
            if (m_lastKnownValues.count(col_name)) finalRow[col_name] = m_lastKnownValues[col_name];
            else {
                if (col_config.type == AggregationType::INVERTER) finalRow[col_name] = "null"; 
                else finalRow[col_name] = "0"; 
            }
        }
    }

    // Aggiunta delle statistiche alla riga
    for (const auto& sensor : m_targetSensors) {
        finalRow[sensor + "_MEAN"] = formatDouble(m_currentStats[sensor].mean, 3);
        finalRow[sensor + "_VAR"]  = formatDouble(m_currentStats[sensor].variance, 3);
        finalRow[sensor + "_STD"]  = formatDouble(m_currentStats[sensor].stdDev, 3);
    }

    if (m_onRowReadyCallback) {
        m_onRowReadyCallback(finalRow);
    }
    
    m_currentRowData.clear();
}

void DataAggregator::flush() {
    finalizeAndEmitRow();
}