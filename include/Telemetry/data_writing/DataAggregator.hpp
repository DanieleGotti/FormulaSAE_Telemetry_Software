#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../data_acquisition/PacketParser.hpp"

// Definisce il formato con cui il valore numerico deve essere convertito in stringa
enum class OutputFormat {
    INTEGER,
    DOUBLE,
    STRING
};

// Definisce come devono essere aggregati i valori per colonna
enum class AggregationType {
    AVERAGE,    // Calcola la media dei valori numerici
    LAST,       // Prende l'ultimo valore ricevuto
    INVERTER    // Concatena gli stati e propaga l'ultimo
};

// Configurazione per una singola colonna del database
struct ColumnConfig {
    std::string name;
    AggregationType type;
    OutputFormat format;
};

// Mappa per rappresentare una riga del database 
using DbRow = std::map<std::string, std::string>;

class DataAggregator {
public:
    DataAggregator(std::vector<ColumnConfig> config, std::function<void(const DbRow&)> onRowReady);
    // Elabora un singolo pacchetto in arrivo
    void processPacket(const PacketParser& packet);
    void flush();

private:
    // Finalizza la riga e la invia 
    void finalizeAndEmitRow();
    // Funzione helper per estrarre un valore numerico da PacketData
    static double getNumericValue(const PacketData& data);
    // Struttura interna per memorizzare i dati durante l'aggregazione
    struct AggregationData {
        std::vector<double> numericValues; // Per AVERAGE
        std::vector<std::string> stringValues; // Per INVERTER
        PacketData lastValue; // Per LAST
    };

    std::chrono::system_clock::time_point m_currentTimestamp;
    std::map<std::string, AggregationData> m_currentRowData;
    
    // Configurazione e stato
    std::vector<ColumnConfig> m_config;
    std::map<std::string, AggregationType> m_configMap;
    std::function<void(const DbRow&)> m_onRowReadyCallback;

    // Memorizza l'ultimo valore noto per ogni colonna 
    std::map<std::string, std::string> m_lastKnownValues; 
};