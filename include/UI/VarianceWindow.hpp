#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <chrono>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_acquisition/PacketParser.hpp"

class UiManager;

class VarianceWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    // Struttura per i 3 dati statistici
    struct StatData {
        double mean = 0.0;
        double variance = 0.0;
        double stdDev = 0.0;
    };

    explicit VarianceWindow(UiManager* uiManager);
    ~VarianceWindow() override = default;

    void draw() override;
    // Riceve le righe del dataset già pronte
    void onAggregatedDataReceived(const DbRow& row) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;

    // Lista dei sensori di cui vogliamo la varianza
    const std::vector<std::string> m_targetSensors = {
        "ACC1A", "ACC2A", "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };

    // I dati statistici calcolati e pronti per essere mostrati a schermo
    std::map<std::string, StatData> m_displayStats;
    // Funzione per estrarre i dati dalla DbRow
    void extractStatsFromRow(const DbRow& row);
    
};