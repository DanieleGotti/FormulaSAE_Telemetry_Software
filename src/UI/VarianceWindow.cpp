#include <imgui.h>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/VarianceWindow.hpp"
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

    auto printStat = [&](const std::string& key) {
        if (hasData && dataToDisplay.count(key)) {
            try {
                double val = std::stod(dataToDisplay.at(key));
                ImGui::Text("%.3f", val);
            } catch (...) {
                ImGui::Text("Null");
            }
        } else {
            ImGui::Text("--");
        }
    };

    if (ImGui::BeginTable("Sensors statistics table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

        ImGui::TableSetupColumn("Sensor");
        ImGui::TableSetupColumn("Mean (\u03BC)");
        ImGui::TableSetupColumn("Variance (\u03C3\u00B2)");
        ImGui::TableSetupColumn("Standard deviation (\u03C3)");
        ImGui::TableHeadersRow();

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

    ImGui::Spacing();
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("");
    ImGui::PopFont();
    ImGui::Separator();

    if (ImGui::BeginTable("Speed statistics table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

        ImGui::TableSetupColumn("Sensor");
        ImGui::TableSetupColumn("Max");
        ImGui::TableSetupColumn("Mean (μ)");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Speed");
        ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[km/h]");

        ImGui::TableSetColumnIndex(1);
        printStat("speed_max");

        ImGui::TableSetColumnIndex(2);
        printStat("speed_mean");

        ImGui::EndTable();
    }
    ImGui::End();
}