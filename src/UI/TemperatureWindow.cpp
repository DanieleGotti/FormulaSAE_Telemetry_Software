/**
 * @file TemperatureWindow.cpp
 * @brief Implementation of the temperatures window (see TemperatureWindow.hpp).
 *
 * The live/playback logic is identical to AccBrkWindow.cpp (see the comments there).
 */
#include "UI/TemperatureWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

// Plot lines: {field key, label}; default Y axis 0..120 °C
TemperatureWindow::TemperatureWindow(UiManager* manager) : m_uiManager(manager),
    m_temperaturePlot("Temperatures", {{ "inv_L_tempMotor",    "Left motor" },
                                       { "inv_R_tempMotor",    "Right motor" },
                                       { "left_coolant_temp",  "Left pump" },
                                       { "right_coolant_temp", "Right pump" }}, 0.0, 120.0)
{
    const std::vector<std::string> keys_to_plot = {
        "inv_L_tempMotor", "inv_R_tempMotor", "left_coolant_temp", "right_coolant_temp"
    };
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void TemperatureWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (!dataRow.count("timestamp")) return;
        double unix_timestamp = std::stod(dataRow.at("timestamp"));

        // Time jump (pause/restart or reset): the series are cleared
        if (m_lastTimestamp >= 0.0 && (unix_timestamp < m_lastTimestamp || unix_timestamp - m_lastTimestamp > 1.5)) {
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
            }
        }
        m_lastTimestamp = unix_timestamp;

        // New sample + drop the ones older than MAX_HISTORY_SECONDS
        for (auto& pair : m_plotData) {
            const std::string& label = pair.first;
            if (dataRow.count(label)) {
                pair.second.X.push_back(unix_timestamp);
                pair.second.Y.push_back(std::stod(dataRow.at(label)));
                while (!pair.second.X.empty() && pair.second.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    pair.second.X.erase(pair.second.X.begin());
                    pair.second.Y.erase(pair.second.Y.begin());
                }
            }
        }
    } catch (const std::exception& e) {}
}

void TemperatureWindow::draw() {
    double cursor_time = 0.0;
    bool isPlayback = m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK;

    if (isPlayback) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto currentRowOpt = playbackManager->getCurrentRow()) {
            if (currentRowOpt->count("timestamp")) {
                try { cursor_time = std::stod(currentRowOpt->at("timestamp")); } catch(...) {}
            }
        }

        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_playbackDataLoaded && playbackManager->getTotalRows() > 0) {

            // Copy the full session series for ImPlot, only once
            std::vector<double> timeline = playbackManager->getPlotTimeline();

            for (auto& pair : m_plotData) {
                pair.second.X = timeline;
                pair.second.Y = playbackManager->getPlotData(pair.first);
            }
            m_playbackDataLoaded = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_playbackDataLoaded = false; // reset when switching modes
        if (!m_plotData.empty() && !m_plotData.begin()->second.X.empty()) {
            cursor_time = m_plotData.begin()->second.X.back();
        }
    }

    ImGui::Begin("Temperatures", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("Motors and water pumps");
    ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[°C]");
    ImGui::PopFont();
    ImGui::Separator();

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_temperaturePlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }

    ImGui::End();
}
