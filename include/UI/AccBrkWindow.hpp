#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "UIElement.hpp"
#include "PlotGraph.hpp" // Usa la nuova classe
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
    const double MAX_HISTORY_SECONDS = 15.0; // Sufficiente per una finestra di 10s
    double m_lastTimestamp = -1.0; // NUOVO: traccia i salti temporali
    
    // Istanze gestite in automatico
    PlotGraph m_accPlot;
    PlotGraph m_brkPlot;
    
    bool m_playbackDataLoaded = false;
};