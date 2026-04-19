#pragma once
#include <map>
#include <string>
#include <vector>
#include <functional>

using DbRow = std::map<std::string, std::string>;

class DataAggregator {
public:
    explicit DataAggregator(std::function<void(const DbRow&)> onRowReady);
    
    // Riceve la riga grezza da PacketParser. Vi appende le varianze.
    void processRow(DbRow row);
    void reset();

private:
    struct StatData {
        double mean = 0.0;
        double variance = 0.0;
        double stdDev = 0.0;
    };
    StatData calculateStats(const std::vector<double>& values) const;

    std::function<void(const DbRow&)> m_onRowReadyCallback;

    double m_statsWindowStart = -1.0;
    std::map<std::string, std::vector<double>> m_statsBuffers;
    std::map<std::string, StatData> m_currentStats;

    // Sensori di cui calcoliamo le statistiche
    const std::vector<std::string> m_targetSensors = {
        "ACC1", "ACC2", "BRK1", "BRK2", "STEER"
    };
    const double STATS_WINDOW = 3.0; // Secondi
};