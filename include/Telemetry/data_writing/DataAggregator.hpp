/**
 * @file DataAggregator.hpp
 * @brief Enriches every live row with statistics and distance travelled, then forwards it.
 *
 * @details
 * Sits between PacketParser and DataManager. For every row received it adds:
 *  - `<channel>_mean`, `<channel>_var`, `<channel>_std` for the channels of
 *    TelemetrySchema::varianceChannels(), computed over 3 s "tumbling" windows
 *    (the published value is the one of the last CLOSED window; "0.000" during
 *    the first 3 s);
 *  - `<channel>_max`, `<channel>_mean` for the channels of
 *    TelemetrySchema::maxMeanChannels(), cumulative over the whole session
 *    (updated every time a window closes);
 *  - `meters`, the distance integrated from the mean speed of the rear wheels.
 *
 * The same statistics are recomputed for files by
 * FileService::computeMissingStats(): if a formula changes here, it must be
 * changed there too. Current differences between the two: here the variance
 * is the sample variance (divisor n-1) and the window is TIME based (3 s of
 * timestamps); for files the variance is the population variance (divisor n)
 * and the window is 600 ROWS.
 */
#pragma once
#include <map>
#include <string>
#include <vector>
#include <limits>
#include <functional>
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"

/**
 * @brief Computes streaming statistics on the live rows and passes them to a callback.
 *
 * Not thread-safe: it is called only by the SerialService processing thread.
 */
class DataAggregator {
public:
    /**
     * @brief Creates the aggregator.
     * @param onRowReady Function called with every complete row (in practice DataManager::processData()).
     */
    explicit DataAggregator(std::function<void(const DbRow&)> onRowReady);
    /**
     * @brief Processes a row: accumulates the values, closes the window after 3 s,
     *        writes the statistic columns and calls the callback.
     * @param row Row produced by the parser (copied and enriched). Ignored without `timestamp`.
     */
    void processRow(DbRow row);
    /** @brief Resets windows, session statistics and distance (new session). */
    void reset();

private:
    /// Mean, sample variance and standard deviation of a window.
    struct StatData {
        double mean = 0.0;
        double variance = 0.0;
        double stdDev = 0.0;
    };
    /**
     * @brief Computes mean, sample variance (divisor n-1) and standard deviation.
     * @param values Window samples.
     * @return Statistics (all 0 with fewer than 2 samples).
     */
    StatData calculateStats(const std::vector<double>& values) const;

    /// Accumulator for session-cumulative max/mean (no rolling window).
    struct RunningStats {
        double max = std::numeric_limits<double>::lowest();
        double sum = 0.0;
        size_t count = 0;
    };
    /**
     * @brief Adds a sample to the accumulator of the current window.
     * @param key   Channel name (e.g. "speed").
     * @param value Sample value.
     */
    void accumulate(const std::string& key, double value);

    std::function<void(const DbRow&)> m_onRowReadyCallback;  ///< Destination of the complete rows.

    double m_statsWindowStart = -1.0;  ///< Start (s) of the current 3 s window; -1 = not started.
    
    /// Samples accumulated in the current window, per channel.
    std::map<std::string, std::vector<double>> m_currentBuffers;
    
    /// Latest computed statistics (closed window), republished on every row for UI and CSV.
    std::map<std::string, StatData> m_lastPublishedStats;

    /// Max/mean accumulators of the current window (filled on every row)...
    std::map<std::string, RunningStats> m_currentAgg;
    /// ...and session totals (updated every time a window closes).
    std::map<std::string, RunningStats> m_sessionAgg;

    double m_distanceMeters = 0.0;   ///< Distance travelled since the start of the session (m).
    double m_lastDistanceTs = -1.0;  ///< Timestamp of the last integrated sample; -1 = none.

    // Both lists come from the schema: adding a channel there is enough
    const std::vector<std::string>& m_targetSensors = TelemetrySchema::varianceChannels();  ///< Channels with mean/variance/std.
    const std::vector<std::string>& m_maxMeanSensors = TelemetrySchema::maxMeanChannels();  ///< Channels with session max/mean.
    const double STATS_WINDOW = 3.0; ///< Length of the statistics window, in seconds.
};