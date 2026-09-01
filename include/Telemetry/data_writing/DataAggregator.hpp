#pragma once
#include <map>
#include <string>
#include <vector>
#include <limits>
#include <functional>
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"

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

    // Session-cumulative max/mean accumulator (no rolling window: it spans the whole session)
    struct RunningStats {
        double max = std::numeric_limits<double>::lowest();
        double sum = 0.0;
        size_t count = 0;
    };
    void accumulate(const std::string& key, double value);

    std::function<void(const DbRow&)> m_onRowReadyCallback;

    double m_statsWindowStart = -1.0;
    
    // Buffer for accumulating data over 3 seconds
    std::map<std::string, std::vector<double>> m_currentBuffers;
    
    // Mantain last valid statistics for printing in UI and CSV
    std::map<std::string, StatData> m_lastPublishedStats;

    // Max/mean accumulators: filled every row, folded into the session totals every window
    std::map<std::string, RunningStats> m_currentAgg;
    std::map<std::string, RunningStats> m_sessionAgg;

    // Distance travelled, integrated sample by sample from the speedometer
    double m_distanceMeters = 0.0;
    double m_lastDistanceTs = -1.0;

    // Both lists come from the schema so that adding a channel there is enough
    const std::vector<std::string>& m_targetSensors = TelemetrySchema::varianceChannels();
    const std::vector<std::string>& m_maxMeanSensors = TelemetrySchema::maxMeanChannels();
    const double STATS_WINDOW = 3.0; // expected time window for stats calculation in seconds
};