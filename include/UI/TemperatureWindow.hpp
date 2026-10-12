/**
 * @file TemperatureWindow.hpp
 * @brief "Temperatures" window: plot of the motor and water pump temperatures.
 *
 * @details
 * Channels used: `inv_L_tempMotor`, `inv_R_tempMotor`, `left_coolant_temp`,
 * `right_coolant_temp`. Same logic as AccBrkWindow.
 * It is a good starting point to copy when creating a new plot window.
 */
#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "UIElement.hpp"
#include "PlotGraph.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager;

/**
 * @brief Motor and water pump temperatures plotted against time.
 */
class TemperatureWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit TemperatureWindow(UiManager* manager);
    ~TemperatureWindow() override = default;

    /** @brief Draws the plot. */
    void draw() override;
    /** @brief Appends the new samples to the series (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;                          ///< Owner.
    std::mutex m_dataMutex;                          ///< Protects m_plotData.
    std::map<std::string, PlotLineData> m_plotData;  ///< Series per key.
    const double MAX_HISTORY_SECONDS = 15.0;         ///< History kept in live mode (s).
    double m_lastTimestamp = -1.0; ///< Last received timestamp, used to detect time jumps.

    PlotGraph m_temperaturePlot;  ///< Temperature plot.

    bool m_playbackDataLoaded = false;  ///< true once the playback series have been copied.
};
