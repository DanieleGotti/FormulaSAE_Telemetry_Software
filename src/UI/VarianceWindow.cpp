#include <imgui.h>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "UI/VarianceWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

VarianceWindow::VarianceWindow(UiManager* manager) : m_uiManager(manager) {
    // Inizializza le mappe
    for (const auto& sensor : m_targetSensors) {
        m_rawBuffers[sensor] = std::vector<double>();
        m_displayStats[sensor] = StatData();
    }
}

VarianceWindow::StatData VarianceWindow::calculateStats(const std::vector<double>& values) const {
    StatData stats;
    if (values.size() < 2) return stats; // Impossibile calcolare con < 2 campioni
    
    // Calcolo media
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    stats.mean = sum / values.size();
    
    // Calcolo varianza
    double variance_sum = 0.0;
    for (double val : values) {
        variance_sum += (val - stats.mean) * (val - stats.mean);
    }
    stats.variance = variance_sum / (values.size() - 1); 
    
    // Calcolo deviazione standard 
    stats.stdDev = std::sqrt(stats.variance);
    
    return stats;
}

void VarianceWindow::onDataReceived(const PacketParser& packet) {
    if (std::find(m_targetSensors.begin(), m_targetSensors.end(), packet.label) == m_targetSensors.end()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_dataMutex);

    if (m_windowStart.time_since_epoch().count() == 0) {
        m_windowStart = packet.timestamp;
    }

    double val = 0.0;
    if (std::holds_alternative<double>(packet.data)) val = std::get<double>(packet.data);
    else if (std::holds_alternative<int>(packet.data)) val = static_cast<double>(std::get<int>(packet.data));
    
    m_rawBuffers[packet.label].push_back(val);

    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(packet.timestamp - m_windowStart).count();
    if (elapsed_seconds >= 5) {
        for (const auto& sensor : m_targetSensors) {
            m_displayStats[sensor] = calculateStats(m_rawBuffers[sensor]);
            m_rawBuffers[sensor].clear();
        }
        m_windowStart = packet.timestamp;
    }
}

void VarianceWindow::draw() {
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (playbackManager) {
            size_t currentIndex = playbackManager->getCurrentIndex();
            auto currentRowOpt = playbackManager->getCurrentRow();
            
            if (currentRowOpt) {
                auto center_ts = PacketParser::parseTimestampString(currentRowOpt->at("timestamp"));
                double centerTime = std::chrono::duration<double>(center_ts.time_since_epoch()).count();

                // Guarda indietro di X secondi rispetto al cursore attuale
                double startTime = centerTime - STATS_WINDOW;
                auto dataWindow = playbackManager->getDataWindow(currentIndex, 500);

                std::map<std::string, std::vector<double>> playbackBuffers;
                for (const auto& row : dataWindow) {
                    auto ts = PacketParser::parseTimestampString(row.at("timestamp"));
                    double unix_ts = std::chrono::duration<double>(ts.time_since_epoch()).count();

                    if (unix_ts >= startTime && unix_ts <= centerTime) {
                        for (const auto& sensor : m_targetSensors) {
                            if (row.count(sensor)) {
                                try {
                                    playbackBuffers[sensor].push_back(std::stod(row.at(sensor)));
                                } catch (...) {}
                            }
                        }
                    }
                }

                std::lock_guard<std::mutex> lock(m_dataMutex);
                for (const auto& sensor : m_targetSensors) {
                    m_displayStats[sensor] = calculateStats(playbackBuffers[sensor]);
                }
            }
        }
    }

    ImGui::Begin("Statistiche sensori", nullptr);

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Aggiornamento: %d secondi", STATS_WINDOW);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    // Tabella a 4 colonne (Sensore, Media, Varianza, StdDev)
    if (ImGui::BeginTable("StatisticheTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Sensore", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Media (\u03BC)");
        ImGui::TableSetupColumn("Varianza (\u03C3\u00B2)");
        ImGui::TableSetupColumn("Deviazione standard (\u03C3)");
        ImGui::TableHeadersRow();

        auto formatValue =[](double val) {
            ImGui::Text("%.3f", val); // 3 decimali per facilità di lettura
        };

        std::lock_guard<std::mutex> lock(m_dataMutex);
        for (const auto& sensor : m_targetSensors) {
            ImGui::TableNextRow();
            
            const StatData& stats = m_displayStats[sensor];

            // Sensore
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sensor.c_str());
            
            // Media
            ImGui::TableSetColumnIndex(1);
            formatValue(stats.mean);

            // Varianza
            ImGui::TableSetColumnIndex(2);
            formatValue(stats.variance);

            // Deviazione standard
            ImGui::TableSetColumnIndex(3);
            formatValue(stats.stdDev);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}