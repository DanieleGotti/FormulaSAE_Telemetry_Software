#include "UI/AccBrkWindow.hpp" 
#include "UI/UIManager.hpp" 
#include "Telemetry/Services/ServiceManager.hpp" 
#include "Telemetry/file_reading/PlaybackManager.hpp"

AccBrkWindow::AccBrkWindow(UiManager* manager) : m_uiManager(manager), 
    m_accPlot("Accelerators", {{ "accelerator1", "1" }, { "accelerator2", "2" }, { "accelerator_mapped", "Mapped" }}, 0.0, 100.0), 
    m_brkPlot("Brakes", {{ "brake1", "1" }, { "brake2", "2" }}, 0.0, 100.0) 
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

        // Detect temporal gaps (pause/restart or system reset)
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
            
            // Fast DOD extraction for ImPlot
            std::vector<double> timeline = playbackManager->getPlotTimeline();
            
            for (auto& pair : m_plotData) {
                pair.second.X = timeline;
                pair.second.Y = playbackManager->getPlotData(pair.first);
            }
            m_playbackDataLoaded = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_playbackDataLoaded = false; // Reset when switching modes
        if (!m_plotData.empty() && !m_plotData.begin()->second.X.empty()) {
            cursor_time = m_plotData.begin()->second.X.back();
        }
    }

    ImGui::Begin("Accelerator and brake sensors", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("Accelerator raw value" ); 
    ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[-]");
    ImGui::PopFont();
    ImGui::Separator();
    
    float available_height = ImGui::GetContentRegionAvail().y;
    float non_plot_height = (2 * ImGui::GetTextLineHeightWithSpacing()) + (3 * ImGui::GetStyle().ItemSpacing.y);
    float plot_height = (available_height - non_plot_height) / 2.0f;
    if (plot_height < 100) plot_height = 100;
    
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_accPlot.draw(m_plotData, cursor_time, isPlayback, plot_height);
    }
    
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("Brake raw value" ); 
    ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[-]");
    ImGui::PopFont();
    ImGui::Separator();
    
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_brkPlot.draw(m_plotData, cursor_time, isPlayback, -1.0f);
    }
    
    ImGui::End();
}