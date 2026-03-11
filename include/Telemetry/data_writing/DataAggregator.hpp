#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../data_acquisition/PacketParser.hpp"

#define STATS_WINDOW 3

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
    // Struttura per memorizzare le statistiche di un sensore
    struct StatData {
        double mean = 0.0;
        double variance = 0.0;
        double stdDev = 0.0;
    };

    // Struttura interna per memorizzare i dati durante l'aggregazione
    struct AggregationData {
        std::vector<double> numericValues; // Per AVERAGE
        std::vector<std::string> stringValues; // Per INVERTER
        PacketData lastValue; // Per LAST
    };

    // Finalizza la riga e la invia 
    void finalizeAndEmitRow();
    // Funzione helper per estrarre un valore numerico da PacketData
    static double getNumericValue(const PacketData& data);
    // Calcola le statistiche (media, varianza, deviazione standard) 
    StatData calculateStats(const std::vector<double>& values) const;

    // Configurazione e stato
    std::chrono::system_clock::time_point m_currentTimestamp;
    std::map<std::string, AggregationData> m_currentRowData;
    std::vector<ColumnConfig> m_config;
    std::map<std::string, AggregationType> m_configMap;
    std::function<void(const DbRow&)> m_onRowReadyCallback;
    // Memorizza l'ultimo valore noto per ogni colonna 
    std::map<std::string, std::string> m_lastKnownValues; 

    // Strumenti per statistiche
    std::chrono::system_clock::time_point m_statsWindowStart;
    std::map<std::string, std::vector<double>> m_statsBuffers;
    std::map<std::string, StatData> m_currentStats;
    const std::vector<std::string> m_targetSensors = {
        "ACC1A", "ACC2A", "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };
};