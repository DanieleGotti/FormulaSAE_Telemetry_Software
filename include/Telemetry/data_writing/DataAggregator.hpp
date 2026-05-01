#pragma once
#include <map>
#include <string>
#include <vector>
#include <functional>

using DbRow = std::map<std::string, std::string>;

class DataAggregator {
public:
    explicit DataAggregator(std::function<void(const DbRow&)> onRowReady);
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
    
    // Buffer per accumulare i dati nei 3 secondi
    std::map<std::string, std::vector<double>> m_currentBuffers;
    
    // Mantiene l'ultima statistica valida per stamparla in UI e CSV
    std::map<std::string, StatData> m_lastPublishedStats;

    const std::vector<std::string> m_targetSensors = {
        "accelerator1", "accelerator2", "brake1", "brake2", "steer"
    };
    const double STATS_WINDOW = 3.0; // Secondi esatti
};