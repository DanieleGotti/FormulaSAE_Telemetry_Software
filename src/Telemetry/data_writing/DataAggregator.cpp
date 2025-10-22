#include <sstream>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <cmath> 
#include "Telemetry/data_writing/DataAggregator.hpp"

DataAggregator::DataAggregator(std::vector<ColumnConfig> config, std::function<void(const DbRow&)> onRowReady)
    : m_config(std::move(config)), m_onRowReadyCallback(std::move(onRowReady)) {
    for (const auto& col : m_config) {
        m_configMap[col.name] = col.type;
    }
}

double DataAggregator::getNumericValue(const PacketData& data) {
    if (std::holds_alternative<int>(data)) return static_cast<double>(std::get<int>(data));
    if (std::holds_alternative<double>(data)) return std::get<double>(data);
    return 0.0; 
}

void DataAggregator::processPacket(const PacketParser& packet) {
    // Se è il primo pacchetto in assoluto, impostiamo il timestamp iniziale
    if (m_currentTimestamp.time_since_epoch().count() == 0) {
        m_currentTimestamp = packet.timestamp;
    }

    // Se il timestamp del pacchetto è diverso, la riga precedente è completa
    if (packet.timestamp != m_currentTimestamp) {
        finalizeAndEmitRow();
        m_currentTimestamp = packet.timestamp;
    }

    // Aggiungi i dati del pacchetto corrente all'aggregazione
    auto it = m_configMap.find(packet.label);
    if (it != m_configMap.end()) {
        AggregationData& data = m_currentRowData[packet.label];
        data.lastValue = packet.data;
        data.numericValues.push_back(getNumericValue(packet.data));
        if (std::holds_alternative<std::string>(packet.data)) {
            data.stringValues.push_back(std::get<std::string>(packet.data));
        }
    }
}

void DataAggregator::finalizeAndEmitRow() {
    if (m_currentRowData.empty() && m_currentTimestamp.time_since_epoch().count() == 0) {
        return; 
    }
    
    DbRow finalRow;
    
    // Formatta il timestamp
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
            switch (col_config.type) {
                case AggregationType::AVERAGE: {
                    double sum = std::accumulate(data.numericValues.begin(), data.numericValues.end(), 0.0);
                    double avg = data.numericValues.empty() ? 0.0 : sum / data.numericValues.size();
                    // Arrotonda e casta a intero
                    finalRow[col_name] = std::to_string(static_cast<int>(std::round(avg)));
                    break;
                }
                case AggregationType::LAST:
                    // Casta a intero
                    finalRow[col_name] = std::to_string(static_cast<int>(getNumericValue(data.lastValue)));
                    break;
                case AggregationType::INVERTER: {
                    std::string combined_states;
                    // Unisce tutti gli stati ricevuti in questo timestamp con " / "
                    for (size_t i = 0; i < data.stringValues.size(); ++i) {
                        combined_states += data.stringValues[i] + (i == data.stringValues.size() - 1 ? "" : " / ");
                    }
                    finalRow[col_name] = combined_states;
                    
                    // Se ricevo almeno uno stato, aggiorno l'ultimo stato conosciuto usando solo l'ultimo valore della sequenza
                    if (!data.stringValues.empty()) {
                        if (col_name == "LEFT_INVERTER_FSM") {
                            m_lastLeftInverterState = data.stringValues.back();
                        }
                        if (col_name == "RIGHT_INVERTER_FSM") {
                            m_lastRightInverterState = data.stringValues.back();
                        }
                    }
                    break;
                }
            }
        } else {
            // Nessun dato per questa colonna, gestione dei casi mancanti
            switch (col_config.type) {
                case AggregationType::AVERAGE:
                case AggregationType::LAST:
                    finalRow[col_name] = "0"; 
                    break;
                case AggregationType::INVERTER:
                    // Propaga l'ultimo stato singolo conosciuto
                    if (col_name == "LEFT_INVERTER_FSM") finalRow[col_name] = m_lastLeftInverterState;
                    if (col_name == "RIGHT_INVERTER_FSM") finalRow[col_name] = m_lastRightInverterState;
                    break;
            }
        }
    }

    if (m_onRowReadyCallback) {
        m_onRowReadyCallback(finalRow);
    }
    m_currentRowData.clear();
}

void DataAggregator::flush() {
    finalizeAndEmitRow();
}

