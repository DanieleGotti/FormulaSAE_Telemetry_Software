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

class SuspensionWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit SuspensionWindow(UiManager* manager);
    ~SuspensionWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    std::map<std::string, PlotLineData> m_plotData;
    const double MAX_HISTORY_SECONDS = 15.0;
    
    PlotGraph m_suspPlot;
};