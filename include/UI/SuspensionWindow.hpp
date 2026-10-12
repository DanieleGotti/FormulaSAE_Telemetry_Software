/**
 * @file SuspensionWindow.hpp
 * @brief "Suspension linear potentiometers" window: plot of the front damper travel.
 *
 * @details
 * Channels used: `front_left_suspension`, `front_right_suspension`.
 * Same logic as AccBrkWindow.
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
 * @brief Plot of the two suspension linear potentiometers.
 */
class SuspensionWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit SuspensionWindow(UiManager* manager);
    ~SuspensionWindow() override = default;

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
    
    PlotGraph m_suspPlot;  ///< Suspension plot.

    bool m_playbackDataLoaded = false;  ///< true once the playback series have been copied.
};