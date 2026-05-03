#include <iostream>
#include "UI/SuspensionWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

SuspensionWindow::SuspensionWindow(UiManager* manager) : m_uiManager(manager),
    m_suspPlot("Potenziometri lineari", { { "front_left_suspension", "Front left" }, { "front_right_suspension", "Front right" }}, 0.0, 100.0)
{
    const std::vector<std::string> keys_to_plot = {"front_left_suspension", "front_right_suspension"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void SuspensionWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (!dataRow.count("timestamp")) return;
        double unix_timestamp = std::stod(dataRow.at("timestamp")); 

        // NUOVO: Rileva i salti temporali (pausa/riavvio o reset del sistema)
        if (m_lastTimestamp >= 0.0 && (unix_timestamp < m_lastTimestamp || unix_timestamp - m_lastTimestamp > 1.5)) {
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
            }
        }
        m_lastTimestamp = unix_timestamp;
        
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

void SuspensionWindow::draw() {
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
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
            }
            
            size_t total = playbackManager->getTotalRows();
            for (size_t i = 0; i < total; ++i) {
                auto row = playbackManager->getRowAtIndex(i);
                if (row && row->count("timestamp")) {
                    try {
                        double unix_ts = std::stod(row->at("timestamp"));
                        for (auto& pair : m_plotData) {
                            if (row->count(pair.first)) {
                                pair.second.X.push_back(unix_ts);
                                pair.second.Y.push_back(std::stod(row->at(pair.first)));
                            }
                        }
                    } catch (...) {} 
                }
            }
            m_playbackDataLoaded = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_playbackDataLoaded = false;
        if (!m_plotData.empty() && !m_plotData.begin()->second.X.empty()) {
            cursor_time = m_plotData.begin()->second.X.back();
        }
    }

    ImGui::Begin("Suspension linear potentiometers", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_suspPlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }

    ImGui::End();
}