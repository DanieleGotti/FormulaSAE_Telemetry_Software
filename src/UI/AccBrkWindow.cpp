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

void AccBrkWindow::draw() {
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
            // Svuotiamo i vecchi dati
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
            }
            
            // Estraiamo tutti i dati del file una sola volta
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
                    } catch (...) {} // Ignora righe malformate nel file
                }
            }
            m_playbackDataLoaded = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_playbackDataLoaded = false; // Reset per quando si aprirà un file
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
        m_brkPlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }

    ImGui::End();
}