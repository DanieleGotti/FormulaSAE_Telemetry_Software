#include <sstream>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <cmath> 
#include "Telemetry/data_writing/DataAggregator.hpp"

// Converte un double in stringa con precisione fissa
static std::string formatDouble(double value, int precision) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

DataAggregator::DataAggregator(std::vector<ColumnConfig> config, std::function<void(const DbRow&)> onRowReady)
    : m_config(std::move(config)), m_onRowReadyCallback(std::move(onRowReady)) {
    // Mappa di configurazione per accesso rapido durante processPacket
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
    // Inizializzazione timestamp al primo pacchetto
    if (m_currentTimestamp.time_since_epoch().count() == 0) {
        m_currentTimestamp = packet.timestamp;
    }

    // Se cambia il timestamp, il pacchetto precedente è finito 
    if (packet.timestamp != m_currentTimestamp) {
        finalizeAndEmitRow();
        m_currentTimestamp = packet.timestamp;
    }

    // Accumula i dati solo se la colonna è configurata
    auto it = m_configMap.find(packet.label);
    if (it != m_configMap.end()) {
        AggregationData& data = m_currentRowData[packet.label];
        
        // salva il valore
        data.lastValue = packet.data;
        data.numericValues.push_back(getNumericValue(packet.data));
        if (std::holds_alternative<std::string>(packet.data)) {
            data.stringValues.push_back(std::get<std::string>(packet.data));
        }
    }
}

void DataAggregator::finalizeAndEmitRow() {
    // Se non abbiamo dati e nemmeno un timestamp valido, usciamo
    if (m_currentRowData.empty() && m_currentTimestamp.time_since_epoch().count() == 0) {
        return; 
    }
    
    DbRow finalRow;
    
    // Formattazione timestamp
    auto in_time_t = std::chrono::system_clock::to_time_t(m_currentTimestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_currentTimestamp.time_since_epoch()) % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    finalRow["timestamp"] = ss.str();

    // Iteriamo su tutte le colonne configurate per decidere cosa scrivere
    for (const auto& col_config : m_config) {
        const std::string& col_name = col_config.name;
        auto it = m_currentRowData.find(col_name);

        // Se abbiamo nuovi dati in questo ciclo temporale
        if (it != m_currentRowData.end()) {
            const AggregationData& data = it->second;
            std::string valToEmit;   // Cosa scriviamo nel database/UI ora
            std::string valToLatch;  // Cosa memorizziamo per il futuro

            switch (col_config.type) {
                case AggregationType::AVERAGE: {
                    double sum = std::accumulate(data.numericValues.begin(), data.numericValues.end(), 0.0);
                    double avg = data.numericValues.empty() ? 0.0 : sum / data.numericValues.size();
                    
                    if (col_config.format == OutputFormat::INTEGER) {
                        valToEmit = std::to_string(static_cast<int>(std::round(avg)));
                    } else {
                        valToEmit = formatDouble(avg, 1);
                    }
                    valToLatch = valToEmit; // Per la media, emettiamo e ricordiamo lo stesso valore
                    break;
                }
                case AggregationType::LAST: {
                    double last_val = getNumericValue(data.lastValue);
                    
                    if (col_config.format == OutputFormat::INTEGER) {
                        valToEmit = std::to_string(static_cast<int>(last_val));
                    } else {
                        valToEmit = formatDouble(last_val, 1); 
                    }
                    valToLatch = valToEmit; // Per last emettiamo e ricordiamo lo stesso valore
                    break;
                }
                case AggregationType::INVERTER: {
                    // Esempio: "OFF / READY / ENABLE"
                    std::string combined_states;
                    for (size_t i = 0; i < data.stringValues.size(); ++i) {
                        combined_states += data.stringValues[i] + (i == data.stringValues.size() - 1 ? "" : " / ");
                    }
                    valToEmit = combined_states;
                    
                    // Salva solo l'ultimo stato (es. "ENABLE")
                    if (!data.stringValues.empty()) {
                        valToLatch = data.stringValues.back();
                    } else {
                         if (m_lastKnownValues.count(col_name)) valToLatch = m_lastKnownValues[col_name];
                         else valToLatch = "UNKNOWN";
                    }
                    break;
                }
            }

            // Scrittura e memorizzazione
            finalRow[col_name] = valToEmit;
            m_lastKnownValues[col_name] = valToLatch;
        } 
        // Se non riceviamo nessun dato in un pacchetto
        else {
            if (m_lastKnownValues.count(col_name)) {
                // Recupera il valore precedente
                finalRow[col_name] = m_lastKnownValues[col_name];
            } else {
                // Mai ricevuto nulla dall'avvio
                if (col_config.type == AggregationType::INVERTER) {
                    finalRow[col_name] = "null"; 
                } else {
                    finalRow[col_name] = "0"; 
                }
            }
        }
    }

    // Invia la riga completa ai subscriber (UI, File Writer, ecc.)
    if (m_onRowReadyCallback) {
        m_onRowReadyCallback(finalRow);
    }
    
    // Pulisce il buffer dei dati CORRENTI (ma m_lastKnownValues rimane!)
    m_currentRowData.clear();
}

void DataAggregator::flush() {
    finalizeAndEmitRow();
}