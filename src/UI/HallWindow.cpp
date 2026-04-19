#include "UI/HallWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include <iostream>

HallWindow::HallWindow(UiManager* manager) : m_uiManager(manager),
    m_hallPlot("Sensori effetto Hall", {"VELADX", "VELASX", "VELPSX", "VELPDX"}, 0.0, 120.0)
{
    const std::vector<std::string> keys_to_plot = {"VELADX", "VELASX", "VELPSX", "VELPDX"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void HallWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (!dataRow.count("timestamp")) return;
        double unix_timestamp = std::stod(dataRow.at("timestamp")); 
        
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

void HallWindow::draw() {
    double cursor_time = 0.0;
    bool isPlayback = m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK;

    if (isPlayback) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        size_t currentIndex = playbackManager->getCurrentIndex();
        auto currentRowOpt = playbackManager->getCurrentRow();
        
        if (currentRowOpt && currentRowOpt->count("timestamp")) {
            cursor_time = std::stod(currentRowOpt->at("timestamp"));
            double startTime = cursor_time - 5.0;
            double endTime = cursor_time + 5.0;
            auto dataWindow = playbackManager->getDataWindow(currentIndex, 2500);

            std::lock_guard<std::mutex> lock(m_dataMutex);
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
                const std::string& label = pair.first;
                for (const auto& row : dataWindow) {
                    if (row.count("timestamp") && row.count(label)) {
                        double unix_ts = std::stod(row.at("timestamp"));
                        if (unix_ts >= startTime && unix_ts <= endTime) {
                            pair.second.X.push_back(unix_ts);
                            pair.second.Y.push_back(std::stod(row.at(label)));
                        }
                    }
                }
            }
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_plotData.empty() && !m_plotData.begin()->second.X.empty()) {
            cursor_time = m_plotData.begin()->second.X.back();
        }
    }

    ImGui::Begin("Velocità", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Sensori effetto Hall");
    ImGui::PopFont();

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_hallPlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }
    ImGui::End();
}