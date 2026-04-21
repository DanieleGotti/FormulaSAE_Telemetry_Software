#include "UI/AccBrkWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include <iostream>

AccBrkWindow::AccBrkWindow(UiManager* manager) : m_uiManager(manager),
    // Passiamo qui le variabili esatte decise
    m_accPlot("Acceleratori", {{ "accelerator1", "Accelerator1" }, { "accelerator2", "Accelerator2" }, { "accelerator_mapped", "Accelerator mapped" }}, 0.0, 100.0),
    m_brkPlot("Freni", {{ "brake1", "Brake1" }, { "brake2", "Brake2" }}, 0.0, 100.0)
{
    const std::vector<std::string> keys_to_plot = {"accelerator1", "accelerator2", "accelerator_mapped", "brake1", "brake2"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void AccBrkWindow::onAggregatedDataReceived(const DbRow& dataRow) {
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

void AccBrkWindow::draw() {
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
            // Carica campioni sufficienti per 10 secondi completi (2500 per star larghi a 5ms)
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

    ImGui::Begin("Accelerator and brake sensors", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    float available_height = ImGui::GetContentRegionAvail().y;
    float non_plot_height = (2 * ImGui::GetTextLineHeightWithSpacing()) + (3 * ImGui::GetStyle().ItemSpacing.y);
    float plot_height = (available_height - non_plot_height) / 2.0f;
    if (plot_height < 100) plot_height = 100;

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_accPlot.draw(m_plotData, cursor_time, isPlayback, plot_height);
    }

    ImGui::Spacing();

    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_brkPlot.draw(m_plotData, cursor_time, isPlayback, plot_height);
    }

    ImGui::End();
}