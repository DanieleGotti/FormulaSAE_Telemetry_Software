#include <imgui.h>
#include <implot.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <iostream>
#include <numeric> 
#include "UI/HallWindow.hpp"
#include "UI/UIManager.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Formattatore per l'asse temporale (condiviso con altre finestre)
struct TimeFormatterData {
    double AxisSpan;
};

static int TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    TimeFormatterData* data = static_cast<TimeFormatterData*>(user_data);
    time_t time = static_cast<time_t>(value);
    
#ifdef _WIN32
    tm tm_info;
    localtime_s(&tm_info, &time);
#else
    tm tm_info = *std::localtime(&time);
#endif

    if (data != nullptr && data->AxisSpan < 2.0) {
        size_t len = strftime(buff, size, "%H:%M:%S", &tm_info);
        double fractional_part = value - static_cast<double>(time);
        int tenths = static_cast<int>(fractional_part * 10.0);
        snprintf(buff + len, size - len, ".%d", tenths);
        return strlen(buff);
    } else {
        return strftime(buff, size, "%H:%M:%S", &tm_info);
    }
}

HallWindow::HallWindow(UiManager* manager) : m_uiManager(manager) {
    const std::vector<std::string> keys_to_plot = {
        "VELADX", "VELASX", "VELPSX", "VELPDX"
    };
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

std::chrono::system_clock::time_point HallWindow::parseTimestamp(const std::string& ts_str) {
    std::tm tm = {};
    std::stringstream ss(ts_str.substr(0, 19)); // "YYYY-MM-DDTHH:MM:SS"
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    try {
        int ms = std::stoi(ts_str.substr(20, 3));
        time_point += std::chrono::milliseconds(ms);
    } catch (const std::exception& e) {
        std::cerr << "ERRORE [HallWindow]: Impossibile ricavare il timestamp per il plot." << std::endl;
    }
    
    return time_point;
}

void HallWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        auto current_timestamp = parseTimestamp(dataRow.at("timestamp"));
        auto duration = current_timestamp.time_since_epoch();
        double unix_timestamp = std::chrono::duration<double>(duration).count();

        double sum = 0.0;
        int count = 0;

        for (auto& pair : m_plotData) {
            const std::string& label = pair.first;
            PlotLineData& line_data = pair.second;

            if (dataRow.count(label)) {
                double value = std::stod(dataRow.at(label));
                line_data.X.push_back(unix_timestamp);
                line_data.Y.push_back(value);

                sum += value;
                count++;
                
                while (!line_data.X.empty() && line_data.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    line_data.X.erase(line_data.X.begin());
                    line_data.Y.erase(line_data.Y.begin());
                }
            }
        }
        
        if (count > 0) {
            m_currentSpeed = static_cast<float>(sum / count);
        }

    } catch (const std::exception& e) {
        std::cerr << "ERRORE [HallWindow]: Errore durante l'elaborazione dei dati per il plot." << std::endl;
    }
}

void HallWindow::drawSpeedometer(float speed) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 available_space = ImGui::GetContentRegionAvail();

    // Calcolo dinamico di raggio e centro 
    float limiting_dim = std::min(available_space.x, available_space.y);
    float radius = (limiting_dim / 2.0f); 
    if (radius < 20.0f) radius = 20.0f; 
    ImVec2 center = ImVec2(window_pos.x + available_space.x * 0.5f, window_pos.y + available_space.y * 0.5f + 20);

    // Disegno del tachimetro 
    float start_angle = (float)M_PI;        
    float end_angle = 2.0f * (float)M_PI;    
    float speed_normalized = std::fmax(0.0f, std::fmin(300.0f, speed)) / 300.0f;
    float speed_angle = start_angle + speed_normalized * (float)M_PI;
    int num_segments = 20;

    // Sfondo del tachimetro
    draw_list->PathClear();
    for (int i = 0; i <= num_segments; ++i) {
        float angle = start_angle + ((float)i / num_segments) * ((float)M_PI);
        draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
    }
    draw_list->PathStroke(ImColor(100, 100, 100), false, 3.0f);

    // Indicatore della velocità attuale 
    if (speed_normalized > 0) {
        draw_list->PathClear();
        for (int i = 0; i <= (int)(speed_normalized * num_segments); ++i) {
            float angle = start_angle + ((float)i / num_segments) * ((float)M_PI);
            draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
        }
        draw_list->PathStroke(ImColor(255, 165, 0), false, 5.0f);
    }

    // Lancetta
    draw_list->AddLine(
        center,
        ImVec2(center.x + cos(speed_angle) * (radius - 10), center.y + sin(speed_angle) * (radius - 10)),
        ImColor(255, 0, 0),
        3.0f
    );
    draw_list->AddCircleFilled(center, 5.0f, ImColor(255, 0, 0));

    ImGui::Dummy(available_space);
}


void HallWindow::draw() {
    ImGui::Begin("Velocità", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Pannello tachimetro
    ImGui::BeginChild("TachimetroChild", ImVec2(0, m_speedometerPaneHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(m_uiManager->font_label);
    char angle_text[16];
    snprintf(angle_text, 16, "%.1f km/h", m_currentSpeed);
    ImVec2 text_size = ImGui::CalcTextSize(angle_text);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_size.x) * 0.5f);
    ImGui::TextUnformatted(angle_text);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
    
    drawSpeedometer(m_currentSpeed);
    ImGui::EndChild();

    // Separatore trascinabile 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 0.25f));
    ImGui::Button("##splitter", ImVec2(-1, 8.0f));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive()) {
        m_speedometerPaneHeight += ImGui::GetIO().MouseDelta.y;
        if (m_speedometerPaneHeight < 150.0f) {
            m_speedometerPaneHeight = 150.0f;
        }
        float max_height = ImGui::GetWindowHeight() - 200.0f; 
        if (m_speedometerPaneHeight > max_height) {
            m_speedometerPaneHeight = max_height;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Pannello sensori hall
    ImGui::BeginChild("GraficoChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    
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

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Sensori effetto Hall");
    ImGui::PopFont();

    if (ImPlot::BeginPlot("##SensoriHall", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_NoHighlight);
        
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -20, 100, ImGuiCond_Always); 

        if (!tick_positions.empty()) {
            ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        }

        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");

        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            for (const auto& pair : m_plotData) {
                ImPlot::PlotLine(pair.first.c_str(), pair.second.X.data(), pair.second.Y.data(), pair.second.X.size());
            }
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
    ImGui::EndChild();
    ImGui::End();
}