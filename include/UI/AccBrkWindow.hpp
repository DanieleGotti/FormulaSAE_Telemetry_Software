#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

// Contiene dati di una singola linea di grafico
struct PlotLineData {
    std::vector<double> X, Y;
};

class UiManager;

class AccBrkWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit AccBrkWindow(UiManager* manager);
    ~AccBrkWindow() override = default;

    void draw() override;
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