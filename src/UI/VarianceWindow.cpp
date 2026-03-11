#include <imgui.h>
#include <iostream>
#include <algorithm>
#include "UI/VarianceWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

VarianceWindow::VarianceWindow(UiManager* manager) : m_uiManager(manager) {
    for (const auto& sensor : m_targetSensors) {
        m_displayStats[sensor] = StatData();
    }
}

// Lettura per evitare problemi di formattazione dei numeri con Excel
static double safeRead(const DbRow& row, const std::string& key) {
    std::string val = "";
    
    // Cerca la colonna esatta
    if (row.count(key)) {
        val = row.at(key);
    } 
    // Bugfix Windows: Cerca la colonna con il carriage return attaccato
    else if (row.count(key + "\r")) {
        val = row.at(key + "\r");
    } 
    else {
        return 0.0;
    }

    // Trasforma le eventuali virgole in punti prima di convertire in double
    std::replace(val.begin(), val.end(), ',', '.');

    try {
        return std::stod(val);
    } catch (...) {
        return 0.0;
    }
}

// Estrae i 3 valori per tutti i sensori
void VarianceWindow::extractStatsFromRow(const DbRow& row) {
    for (const auto& sensor : m_targetSensors) {
        m_displayStats[sensor].mean     = safeRead(row, sensor + "_MEAN");
        m_displayStats[sensor].variance = safeRead(row, sensor + "_VAR");
        m_displayStats[sensor].stdDev   = safeRead(row, sensor + "_STD");
    }
}

void VarianceWindow::onAggregatedDataReceived(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    extractStatsFromRow(row);
}

void VarianceWindow::draw() {
    // Modalità PLAYBACK: legge direttamente la riga dove si trova il cursore
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (playbackManager) {
            auto currentRowOpt = playbackManager->getCurrentRow();
            if (currentRowOpt) {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                extractStatsFromRow(*currentRowOpt);
            }
        }
    }

    // Disegna la tabella con i dati correnti
    ImGui::Begin("Statistiche sensori", nullptr);

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Statistiche sensori aggiornate ogni %d secondi", STATS_WINDOW);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("StatisticheTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Sensore", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Media (\u03BC)");
        ImGui::TableSetupColumn("Varianza (\u03C3\u00B2)");
        ImGui::TableSetupColumn("Deviazione standard (\u03C3)");
        ImGui::TableHeadersRow();

        auto formatValue =[](double val) {
            ImGui::Text("%.3f", val); // Formatta sempre con 3 decimali puliti
        };

        std::lock_guard<std::mutex> lock(m_dataMutex);
        for (const auto& sensor : m_targetSensors) {
            ImGui::TableNextRow();
            const StatData& stats = m_displayStats[sensor];

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sensor.c_str());
            
            ImGui::TableSetColumnIndex(1);
            formatValue(stats.mean);

            ImGui::TableSetColumnIndex(2);
            formatValue(stats.variance);

            ImGui::TableSetColumnIndex(3);
            formatValue(stats.stdDev);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}