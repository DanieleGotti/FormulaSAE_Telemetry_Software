#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <chrono>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

// Contiene i dati di una singola linea del grafico
struct PlotLineData;

class UiManager;

class HallWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit HallWindow(UiManager* manager);
    ~HallWindow() override = default;

    // Metodo ereditato da UIElement
    void draw() override;
    // Metodo ereditato da IAggregatedDataSubscriber
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    // Funzione di utilità per convertire la stringa timestamp in un time_point
    std::chrono::system_clock::time_point parseTimestamp(const std::string& ts_str);

    // Disegna il tachimetro a semicerchio
    void drawSpeedometer(float speed);

    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    
    // Mappa per memorizzare i dati di ogni sensore da plottare
    std::map<std::string, PlotLineData> m_plotData;
    const double MAX_HISTORY_SECONDS = 20.0;
    
    // Valore corrente della velocità media
    float m_currentSpeed = 0.0f;
    // Altezza del pannello del tachimetro
    float m_speedometerPaneHeight = 230.0f;
};