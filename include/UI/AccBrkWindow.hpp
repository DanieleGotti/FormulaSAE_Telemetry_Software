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

class AccBrkWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit AccBrkWindow(UiManager* manager);
    ~AccBrkWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    std::map<std::string, PlotLineData> m_plotData;
    const double MAX_HISTORY_SECONDS = 15.0; // Window of 15 seconds for plotting
    double m_lastTimestamp = -1.0; // Track temporal jumps
    
    PlotGraph m_accPlot;
    PlotGraph m_brkPlot;
    
    bool m_playbackDataLoaded = false;
};