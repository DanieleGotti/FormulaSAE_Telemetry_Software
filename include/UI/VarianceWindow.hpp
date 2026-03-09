#pragma once
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <chrono>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IWritingSubscriber.hpp"
#include "../Telemetry/data_acquisition/PacketParser.hpp"

#define STATS_WINDOW 3

class UiManager;

class VarianceWindow : public UIElement, public IWritingSubscriber {
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
    // Callback chiamata dal DataManager (dati raw)
    void onDataReceived(const PacketParser& packet) override;
    // Siccome non scrive file in questa UI ritorna true
    bool createFile(const std::string& directoryPath) override { return true; }

    private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;

    // Lista dei sensori di cui vogliamo la varianza
    const std::vector<std::string> m_targetSensors = {
        "ACC1A", "ACC2A", "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };

    // Buffer per accumulare i dati grezzi nei X secondi (solo per modalità live)
    std::map<std::string, std::vector<double>> m_rawBuffers;
    // I dati statistici calcolati e pronti per essere mostrati a schermo
    std::map<std::string, StatData> m_displayStats;
    // Tracking del tempo per la finestra di calcolo
    std::chrono::system_clock::time_point m_windowStart;
    // Funzione helper matematica
    StatData calculateStats(const std::vector<double>& values) const;
};