#include <imgui.h>
#include "UI/VarianceWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

VarianceWindow::VarianceWindow(UiManager* manager) : m_uiManager(manager) {
    for (const auto& sensor : m_targetSensors) {
        m_displayStats[sensor] = StatData();
    }
}

void VarianceWindow::extractStatsFromRow(const DbRow& row) {
    for (const auto& sensor : m_targetSensors) {
        if (row.count(sensor + "_MEAN")) {
            try { m_displayStats[sensor].mean = std::stod(row.at(sensor + "_MEAN")); } catch (...) {}
        }
        if (row.count(sensor + "_VAR")) {
            try { m_displayStats[sensor].variance = std::stod(row.at(sensor + "_VAR")); } catch (...) {}
        }
        if (row.count(sensor + "_STD")) {
            try { m_displayStats[sensor].stdDev = std::stod(row.at(sensor + "_STD")); } catch (...) {}
        }
    }
}

void VarianceWindow::onAggregatedDataReceived(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    extractStatsFromRow(row);
}

void VarianceWindow::draw() {
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
            ImGui::Text("%.3f", val); 
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