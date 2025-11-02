#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

// Riusa la struttura PlotLineData da AccBrkWindow.hpp
struct PlotLineData; 

class UiManager;

class SuspensionWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit SuspensionWindow(UiManager* manager);
    ~SuspensionWindow() override = default;

    // Metodo da UIElement
    void draw() override;
    // Metodo da IAggregatedDataSubscriber
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    // Helper per convertire il timestamp stringa in un time_point
    std::chrono::system_clock::time_point parseTimestamp(const std::string& ts_str);

    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    // Mappa per memorizzare i dati di ogni sensore da plottare
    std::map<std::string, PlotLineData> m_plotData;
    const double MAX_HISTORY_SECONDS = 20.0;
};