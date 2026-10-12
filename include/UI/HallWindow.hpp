/**
 * @file HallWindow.hpp
 * @brief "Hall sensors" window: plot of the speeds of the four wheels (km/h).
 *
 * @details
 * Channels used: `front_left_velocity`, `front_right_velocity`,
 * `rear_left_velocity`, `rear_right_velocity`. Same logic as AccBrkWindow.
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
 * @brief Single plot with the four wheel speeds.
 */
class HallWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit HallWindow(UiManager* manager);
    ~HallWindow() override = default;

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
    
    PlotGraph m_hallPlot;  ///< Wheel speed plot.

    bool m_playbackDataLoaded = false;  ///< true once the playback series have been copied.
};