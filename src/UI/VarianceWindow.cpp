/**
 * @file VarianceWindow.cpp
 * @brief Implementation of the statistics window (see VarianceWindow.hpp).
 */
#include <imgui.h>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/VarianceWindow.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

/**
 * @brief Human-readable label and unit for the rows of the max/mean table.
 *
 * Channels not listed here simply show their raw schema name with unit "[-]",
 * so adding a channel to the schema breaks nothing. To give it a readable
 * name just add an entry to the `names` map.
 *
 * @param channel Channel name (e.g. "speed").
 * @return (label, unit) pair.
 */
static const std::pair<std::string, std::string>& displayNameFor(const std::string& channel) {
    static const std::map<std::string, std::pair<std::string, std::string>> names = {
        { "speed",              { "Speed",                       "[km/h]" } },
        { "inv_L_tempMotor",    { "Left motor temperature",      "[°C]" } },
        { "inv_R_tempMotor",    { "Right motor temperature",     "[°C]" } },
        { "left_coolant_temp",  { "Left water pump temperature", "[°C]" } },
        { "right_coolant_temp", { "Right water pump temperature","[°C]" } },
    };
    static std::map<std::string, std::pair<std::string, std::string>> fallback;

    const auto it = names.find(channel);
    if (it != names.end()) return it->second;

    auto& entry = fallback[channel];
    if (entry.first.empty()) entry = { channel, "[-]" };
    return entry;
}

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

    // Prints a value with 3 decimals, "--" when missing, "Null" when not numeric
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

    // ---- Table 1: session max and mean ----
    if (ImGui::BeginTable("Speed statistics table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

        ImGui::TableSetupColumn("Sensor");
        ImGui::TableSetupColumn("Max");
        ImGui::TableSetupColumn("Mean (μ)");
        ImGui::TableHeadersRow();

        // Rows follow the schema: a channel added there shows up here too
        for (const auto& channel : TelemetrySchema::maxMeanChannels()) {
            const auto& display = displayNameFor(channel);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(display.first.c_str());
            ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", display.second.c_str());

            ImGui::TableSetColumnIndex(1);
            printStat(channel + "_max");

            ImGui::TableSetColumnIndex(2);
            printStat(channel + "_mean");
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("");
    ImGui::PopFont();
    ImGui::Separator();

    // ---- Table 2: mean / variance / standard deviation over 3 s ----
    if (ImGui::BeginTable("Sensors statistics table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

        ImGui::TableSetupColumn("Sensor");
        ImGui::TableSetupColumn("Mean (μ)");
        ImGui::TableSetupColumn("Variance (σ²)");
        ImGui::TableSetupColumn("Standard deviation (σ)");
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

    ImGui::End();
}