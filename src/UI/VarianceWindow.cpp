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

    ImGui::Begin("Statistiche sensori", nullptr);

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Statistiche sensori aggiornate ogni 3 secondi");
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("StatisticheTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Sensore", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Media (\u03BC)");
        ImGui::TableSetupColumn("Varianza (\u03C3\u00B2)");
        ImGui::TableSetupColumn("Deviazione standard (\u03C3)");
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
            printStat(sensor + "_MEAN");

            ImGui::TableSetColumnIndex(2);
            printStat(sensor + "_VAR");

            ImGui::TableSetColumnIndex(3);
            printStat(sensor + "_STD");
        }
        ImGui::EndTable();
    }
    ImGui::End();
}