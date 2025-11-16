#include <imgui.h>
#include <implot.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <iostream>
#include <limits> 
#include "UI/SuspensionWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

static int TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    time_t time = static_cast<time_t>(value);

#ifdef _WIN32
    tm tm_info; localtime_s(&tm_info, &time);
#else
    tm tm_info = *std::localtime(&time);
#endif
    return (int)strftime(buff, size, "%H:%M:%S", &tm_info);
}

SuspensionWindow::SuspensionWindow(UiManager* manager) : m_uiManager(manager) {
    const std::vector<std::string> keys_to_plot = {"SOSPADX", "SOSPASX", "SOSPPDX", "SOSPPSX"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void SuspensionWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        auto current_timestamp = PacketParser::parseTimestampString(dataRow.at("timestamp"));
        double unix_timestamp = std::chrono::duration<double>(current_timestamp.time_since_epoch()).count();
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
    } catch (const std::exception& e) {
        std::cerr << "ERRORE [SuspensionWindow]: Errore durante l'elaborazione dei dati per il plot." << e.what() << std::endl;
    }
}

void SuspensionWindow::draw() {
    // Modalità lettura file
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        size_t currentIndex = playbackManager->getCurrentIndex();
        auto currentRowOpt = playbackManager->getCurrentRow();
        if (currentRowOpt) {
            auto center_ts = PacketParser::parseTimestampString(currentRowOpt->at("timestamp"));
            double centerTime = std::chrono::duration<double>(center_ts.time_since_epoch()).count();
            double startTime = centerTime - (MAX_HISTORY_SECONDS / 2.0);
            double endTime = centerTime + (MAX_HISTORY_SECONDS / 2.0);
            auto dataWindow = playbackManager->getDataWindow(currentIndex, 1000);
            for (auto& pair : m_plotData) {
                pair.second.X.clear();
                pair.second.Y.clear();
                const std::string& label = pair.first;
                for (const auto& row : dataWindow) {
                    auto ts = PacketParser::parseTimestampString(row.at("timestamp"));
                    double unix_ts = std::chrono::duration<double>(ts.time_since_epoch()).count();
                    if (unix_ts >= startTime && unix_ts <= endTime && row.count(label)) {
                        pair.second.X.push_back(unix_ts);
                        pair.second.Y.push_back(std::stod(row.at(label)));
                    }
                }
            }
        }
    }

    // Disegno della finestra
    ImGui::Begin("Sospensioni", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    float plot_height = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
    if (plot_height < 100) plot_height = 100;
    
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_NoHighlight;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_NoHighlight; 

    // Calcolo dell'asse X
    double window_start_time, now_time;
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto currentRowOpt = ServiceManager::getPlaybackManager()->getCurrentRow();
        if (currentRowOpt) {
            auto center_ts = PacketParser::parseTimestampString(currentRowOpt->at("timestamp"));
            double centerTime = std::chrono::duration<double>(center_ts.time_since_epoch()).count();
            window_start_time = centerTime - (MAX_HISTORY_SECONDS / 2.0);
            now_time = centerTime + (MAX_HISTORY_SECONDS / 2.0);
        } else {
            window_start_time = 0; now_time = 1;
        }
    } else {
        // Modalità live
        now_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        window_start_time = now_time - 10.0;
    }

    // Calcolo dell'asse X
    float plot_width_pixels = ImGui::GetContentRegionAvail().x;
    if (plot_width_pixels <= 0) plot_width_pixels = 1;
    const double nice_steps[] = {1.0, 2.0, 5.0, 10.0};
    const float min_pixels_per_label = 80.0f;
    double chosen_step = 1.0;
    for (double step : nice_steps) {
        float pixels_per_step = (step / (now_time - window_start_time)) * plot_width_pixels;
        if (pixels_per_step > min_pixels_per_label) {
            chosen_step = step;
            break;
        }
    }
    std::vector<double> tick_positions;
    std::vector<const char*> tick_labels;
    static char labels_buffer[20][32];
    int label_idx = 0;
    double first_tick = std::floor(window_start_time / chosen_step) * chosen_step;
    for (double sec = first_tick; sec <= now_time + chosen_step; sec += chosen_step) {
        if (label_idx >= 20) break;
        if (sec >= window_start_time) {
            tick_positions.push_back(sec);
            TimeAxisFormatter(sec, labels_buffer[label_idx], 32, nullptr);
            tick_labels.push_back(labels_buffer[label_idx]);
            label_idx++;
        }
    }

    // Grafico sospensioni
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Potenziometri lineari");
    ImGui::PopFont();
    
    if (ImPlot::BeginPlot("##Sospensioni", ImVec2(-1, plot_height), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, x_flags, y_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always); 
        if (!tick_positions.empty()) ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr); 
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");
        
        {
            std::lock_guard<std::mutex> lock(m_dataMutex); 
            double min_y = std::numeric_limits<double>::max();
            double max_y = std::numeric_limits<double>::lowest();
            bool has_data = false;
            const std::vector<std::string> susp_keys = {"SOSPADX", "SOSPASX", "SOSPPDX", "SOSPPSX"};
            for (const auto& key : susp_keys) {
                const auto& line_data = m_plotData.at(key);
                 for (size_t i = 0; i < line_data.X.size(); ++i) {
                    if (line_data.X[i] >= window_start_time) {
                        if (line_data.Y[i] < min_y) min_y = line_data.Y[i];
                        if (line_data.Y[i] > max_y) max_y = line_data.Y[i];
                        has_data = true;
                    }
                }
            }
            if (has_data) {
                double final_min_y = -3.5, final_max_y = 3.5;
                final_min_y = (std::min)(final_min_y, min_y);
                final_max_y = (std::max)(final_max_y, max_y);
                double range = final_max_y - final_min_y;
                double padding = range * 0.10; 
                ImPlot::SetupAxisLimits(ImAxis_Y1, final_min_y - padding, final_max_y + padding, ImGuiCond_Always);
            } else {
                ImPlot::SetupAxisLimits(ImAxis_Y1, -3.5, 3.5, ImGuiCond_Always);
            }

            ImPlot::PlotLine("SOSPADX", m_plotData.at("SOSPADX").X.data(), m_plotData.at("SOSPADX").Y.data(), m_plotData.at("SOSPADX").X.size());
            ImPlot::PlotLine("SOSPASX", m_plotData.at("SOSPASX").X.data(), m_plotData.at("SOSPASX").Y.data(), m_plotData.at("SOSPASX").X.size());
            ImPlot::PlotLine("SOSPPDX", m_plotData.at("SOSPPDX").X.data(), m_plotData.at("SOSPPDX").Y.data(), m_plotData.at("SOSPPDX").X.size());
            ImPlot::PlotLine("SOSPPSX", m_plotData.at("SOSPPSX").X.data(), m_plotData.at("SOSPPSX").Y.data(), m_plotData.at("SOSPPSX").X.size());
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }

    ImGui::End();
}