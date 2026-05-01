#include <imgui.h>
#include <string>
#include "UI/VarianceWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

VarianceWindow::VarianceWindow(UiManager* manager) : m_uiManager(manager) {}

void VarianceWindow::onAggregatedDataReceived(const DbRow& row) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = row;
}

void VarianceWindow::draw() {
    DbRow dataToDisplay;
    bool hasData = false;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (playbackManager) {
            if (auto currentRowOpt = playbackManager->getCurrentRow()) {
                dataToDisplay = *currentRowOpt;
                hasData = true;
            }
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_latestData.empty()) {
            dataToDisplay = m_latestData;
            hasData = true;
        }
    }

    ImGui::Begin("Sensor statistics (3 s)", nullptr);

    // Usa SizingStretchSame per forzare colonne uguali
    if (ImGui::BeginTable("StatisticheTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
        
        // Ho rimosso il WidthFixed dalla prima colonna. Ora tutte e 4 divideranno lo spazio al 25% esatto.
        ImGui::TableSetupColumn("Sensor"); 
        ImGui::TableSetupColumn("Mean (\u03BC)");
        ImGui::TableSetupColumn("Variance (\u03C3\u00B2)");
        ImGui::TableSetupColumn("Standard deviation (\u03C3)");
        ImGui::TableHeadersRow();

        auto printStat = [&](const std::string& key) {
            if (hasData && dataToDisplay.count(key)) {
                try {
                    double val = std::stod(dataToDisplay.at(key));
                    ImGui::Text("%.3f", val);
                } catch (...) {
                    ImGui::Text("N/D");
                }
            } else {
                ImGui::Text("--");
            }
        };

        for (const auto& sensor : m_targetSensors) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sensor.c_str());
            
            ImGui::TableSetColumnIndex(1);
            printStat(sensor + "_mean");

            ImGui::TableSetColumnIndex(2);
            printStat(sensor + "_var");

            ImGui::TableSetColumnIndex(3);
            printStat(sensor + "_std");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}