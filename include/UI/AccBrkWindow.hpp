/**
 * @file AccBrkWindow.hpp
 * @brief "Accelerator and brake sensors" window: plots of the raw accelerator and brake sensors.
 *
 * @details
 * Channels used: `accelerator1`, `accelerator2`, `brake1`, `brake2`.
 * It is also the reference model for the windows with time plots:
 *  - live: received rows are accumulated in m_plotData (last 15 s);
 *  - playback: on the first draw() the full series are copied from the PlaybackManager.
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
 * @brief Two plots (accelerator, brake) with two lines each.
 */
class AccBrkWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager (state, fonts). */
    explicit AccBrkWindow(UiManager* manager);
    ~AccBrkWindow() override = default;

    /** @brief Draws the two plots, splitting the available height. */
    void draw() override;
    /** @brief Appends the new samples to the series (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;                          ///< Owner.
    std::mutex m_dataMutex;                          ///< Protects m_plotData.
    std::map<std::string, PlotLineData> m_plotData;  ///< Series per key (field name).
    const double MAX_HISTORY_SECONDS = 15.0; ///< History kept in live mode (s).
    double m_lastTimestamp = -1.0; ///< Last received timestamp, used to detect time jumps.
    
    PlotGraph m_accPlot;  ///< Accelerator plot.
    PlotGraph m_brkPlot;  ///< Brake plot.
    
    bool m_playbackDataLoaded = false;  ///< true once the playback series have been copied.
};