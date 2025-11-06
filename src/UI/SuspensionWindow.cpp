#include <imgui.h>
#include <implot.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <iostream>
#include "UI/SuspensionWindow.hpp"
#include "UI/UIManager.hpp"

// Formattatore asse temporale
static int TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    time_t time = static_cast<time_t>(value);
#ifdef _WIN32
    tm tm_info;
    localtime_s(&tm_info, &time);
#else
    tm tm_info = *std::localtime(&time);
#endif
    return strftime(buff, size, "%H:%M:%S", &tm_info);
}


SuspensionWindow::SuspensionWindow(UiManager* manager) : m_uiManager(manager) {
    const std::vector<std::string> keys_to_plot = {
        "SOSAD", "SOSAS", "SOSPD", "SOSPS"
    };
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

std::chrono::system_clock::time_point SuspensionWindow::parseTimestamp(const std::string& ts_str) {
    std::tm tm = {};
    std::stringstream ss(ts_str.substr(0, 19)); // "YYYY-MM-DDTHH:MM:SS"
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    try {
        if (ts_str.length() >= 23) {
            int ms = std::stoi(ts_str.substr(20, 3));
            time_point += std::chrono::milliseconds(ms);
        }
    } catch (const std::exception& e) {
        std::cerr << "ERRORE [SuspensionWindow]: Impossibile ricavare il timestamp per il plot." << std::endl;
    }
    
    return time_point;
}

void SuspensionWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        auto current_timestamp = parseTimestamp(dataRow.at("timestamp"));
        auto duration = current_timestamp.time_since_epoch();
        double unix_timestamp = std::chrono::duration<double>(duration).count();

        for (auto& pair : m_plotData) {
            const std::string& label = pair.first;
            PlotLineData& line_data = pair.second;

            if (dataRow.count(label)) {
                line_data.X.push_back(unix_timestamp);
                line_data.Y.push_back(std::stod(dataRow.at(label)));

                // Rimuovi dati vecchi
                while (!line_data.X.empty() && line_data.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    line_data.X.erase(line_data.X.begin());
                    line_data.Y.erase(line_data.Y.begin());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ERRORE [SuspensionWindow]: Errore durante l'elaborazione dei dati per il plot." << e.what() << std::endl;
    }
}

void SuspensionWindow::draw() {
    ImGui::Begin("Sospensioni", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    float plot_height = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
    if (plot_height < 100) plot_height = 100;
    
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_NoHighlight;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_NoHighlight; 

    double now_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    double window_start_time = now_time - 10;

    float plot_width_pixels = ImGui::GetContentRegionAvail().x;
    if (plot_width_pixels <= 0) plot_width_pixels = 1;

    const double nice_steps[] = {1.0, 2.0, 5.0, 10.0};
    const float min_pixels_per_label = 80.0f;
    double chosen_step = 1.0;

    for (double step : nice_steps) {
        float pixels_per_step = (step / 10.0) * plot_width_pixels;
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
        
        ImPlot::SetupAxisLimits(ImAxis_Y1, -4, 4, ImGuiCond_Always);

        if (!tick_positions.empty())
            ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr); 
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");
        
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);

            ImPlot::PlotLine("SOSAD", m_plotData["SOSPADX"].X.data(), m_plotData["SOSPADX"].Y.data(), m_plotData["SOSPADX"].X.size());
            ImPlot::PlotLine("SOSAS", m_plotData["SOSPASX"].X.data(), m_plotData["SOSPASX"].Y.data(), m_plotData["SOSPASX"].X.size());
            ImPlot::PlotLine("SOSPD", m_plotData["SOSPPDX"].X.data(), m_plotData["SOSPPDX"].Y.data(), m_plotData["SOSPPDX"].X.size());
            ImPlot::PlotLine("SOSPS", m_plotData["SOSPPSX"].X.data(), m_plotData["SOSPPSX"].Y.data(), m_plotData["SOSPPSX"].X.size());
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }

    ImGui::End();
}