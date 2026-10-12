/**
 * @file VarianceWindow.hpp
 * @brief "Sensor statistics (3 s)" window: tables of the statistics computed by DataAggregator/FileService.
 *
 * @details
 * - Table 1: session max and mean for the channels of TelemetrySchema::maxMeanChannels().
 * - Table 2: mean, variance and standard deviation (3 s windows) for the
 *   channels of TelemetrySchema::varianceChannels().
 * Rows follow the schema: adding a channel there makes it show up here too.
 * The displayed name and unit are set in `displayNameFor()` (VarianceWindow.cpp).
 */
#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_acquisition/TelemetrySchema.hpp"

class UiManager;

/**
 * @brief Tables of the sensor statistics.
 */
class VarianceWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param uiManager Owning UiManager. */
    explicit VarianceWindow(UiManager* uiManager);
    ~VarianceWindow() override = default;

    /** @brief Draws the two tables ("--" when a value is missing). */
    void draw() override;
    /** @brief Stores the latest live row (serial thread). */
    void onAggregatedDataReceived(const DbRow& row) override;

private:
    UiManager* m_uiManager;   ///< Owner.
    std::mutex m_dataMutex;   ///< Protects m_latestData.
    DbRow m_latestData;       ///< Latest live row received.

    /// Channels with mean/variance/std, sourced from the schema so they can never drift from the DataAggregator.
    const std::vector<std::string>& m_targetSensors = TelemetrySchema::varianceChannels();
};