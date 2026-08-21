#pragma once
#include <map>
#include <string>
#include <vector>
#include <limits>
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

    const std::vector<std::string> m_targetSensors = {
        "accelerator1", "accelerator2", "brake1", "brake2", "steer"
    };

    // Channels published as max/mean only, read straight from the row.
    // "speed" is handled separately because it is derived from the two rear wheels.
    const std::vector<std::string> m_maxMeanSensors = {
        "inv_L_tempMotor", "inv_R_tempMotor", "left_coolant_temp", "right_coolant_temp"
    };
    const double STATS_WINDOW = 3.0; // expected time window for stats calculation in seconds
};