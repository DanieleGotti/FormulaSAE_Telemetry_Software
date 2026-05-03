#include "UI/HallWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include <iostream>

HallWindow::HallWindow(UiManager* manager) : m_uiManager(manager),
    m_hallPlot("Sensori effetto Hall", {{ "front_right_velocity", "Front right" }, { "front_left_velocity", "Front left" }, { "rear_left_velocity", "Rear left" }, { "rear_right_velocity", "Rear right" }}, 0.0, 100.0)
{
    const std::vector<std::string> keys_to_plot = {"front_right_velocity", "front_left_velocity", "rear_left_velocity", "rear_right_velocity"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void HallWindow::onAggregatedDataReceived(const DbRow& dataRow) {
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

void HallWindow::draw() {
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

    ImGui::Begin("Hall sensors", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        // Usa -1.0f come da tua implementazione originale per riempire lo spazio
        m_hallPlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }
    ImGui::End();
}