#include <imgui.h>
#include <implot.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <iostream>
#include <numeric> 
#include <limits> 
#include "UI/HallWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct TimeFormatterData { 
    double AxisSpan; 
};

static int TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    TimeFormatterData* data = static_cast<TimeFormatterData*>(user_data);
    time_t time = static_cast<time_t>(value);

#ifdef _WIN32
    tm tm_info; localtime_s(&tm_info, &time);
#else
    tm tm_info = *std::localtime(&time);
#endif
    if (data != nullptr && data->AxisSpan < 2.0) {
        size_t len = strftime(buff, size, "%H:%M:%S", &tm_info);
        double fractional_part = value - static_cast<double>(time);
        int tenths = static_cast<int>(fractional_part * 10.0);
        snprintf(buff + len, size - len, ".%d", tenths);
        return (int)strlen(buff);
    } else {
        return (int)strftime(buff, size, "%H:%M:%S", &tm_info);
    }
}

HallWindow::HallWindow(UiManager* manager) : m_uiManager(manager) {
    const std::vector<std::string> keys_to_plot = {"VELADX", "VELASX", "VELPSX", "VELPDX"};
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

void HallWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        auto current_timestamp = PacketParser::parseTimestampString(dataRow.at("timestamp"));
        double unix_timestamp = std::chrono::duration<double>(current_timestamp.time_since_epoch()).count();
        double sum = 0.0;
        int count = 0;
        for (auto& pair : m_plotData) {
            const std::string& label = pair.first;
            if (dataRow.count(label)) {
                double value = std::stod(dataRow.at(label));
                pair.second.X.push_back(unix_timestamp);
                pair.second.Y.push_back(value);
                sum += value;
                count++;
                while (!pair.second.X.empty() && pair.second.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    pair.second.X.erase(pair.second.X.begin());
                    pair.second.Y.erase(pair.second.Y.begin());
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
    
    float limiting_dim = std::min(available_space.x, available_space.y);
    float radius = (limiting_dim / 2.0f); 
    if (radius < 20.0f) radius = 20.0f; 

    ImVec2 center = ImVec2(window_pos.x + available_space.x * 0.5f, window_pos.y + available_space.y * 0.5f + radius * 0.25f);
    float start_angle = (float)M_PI;
    float speed_normalized = std::fmax(0.0f, std::fmin(300.0f, speed)) / 300.0f;
    float speed_angle = start_angle + speed_normalized * (float)M_PI;
    int num_segments = 20;
    draw_list->PathClear();

    for (int i = 0; i <= num_segments; ++i) {
        float angle = start_angle + ((float)i / num_segments) * ((float)M_PI);
        draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
    }
    draw_list->PathStroke(ImColor(100, 100, 100), ImDrawFlags_None, 3.0f);

    if (speed_normalized > 0) {
        draw_list->PathClear();
        for (int i = 0; i <= (int)(speed_normalized * num_segments); ++i) {
            float angle = start_angle + ((float)i / num_segments) * ((float)M_PI);
            draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
        }
        draw_list->PathStroke(ImColor(255, 165, 0), ImDrawFlags_None, 5.0f);
    }
    draw_list->AddLine(center, ImVec2(center.x + cos(speed_angle) * (radius - 10), center.y + sin(speed_angle) * (radius - 10)), ImColor(255, 0, 0), 3.0f);
    draw_list->AddCircleFilled(center, 5.0f, ImColor(255, 0, 0));

    // Visualizzazione velocità sotto il tachimetro
    ImGui::PushFont(m_uiManager->font_label);
    char speed_text[16];
    snprintf(speed_text, 16, "%.1f km/h", speed);
    ImVec2 speed_text_size = ImGui::CalcTextSize(speed_text);
    draw_list->AddText(ImVec2(center.x - speed_text_size.x * 0.5f, center.y + 20.0f), ImGui::GetColorU32(ImGuiCol_Text), speed_text);
    ImGui::PopFont();
    ImGui::Dummy(available_space);
}

void HallWindow::draw() {
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
    ImGui::Begin("Velocità", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    float currentSpeed;
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        currentSpeed = 0.0f;
        auto currentRowOpt = ServiceManager::getPlaybackManager()->getCurrentRow();
        if (currentRowOpt) {
            try {
                double sum = 0.0;
                sum += std::stod(currentRowOpt->at("VELADX"));
                sum += std::stod(currentRowOpt->at("VELASX"));
                // sum += std::stod(currentRowOpt->at("VELPSX"));
                // sum += std::stod(currentRowOpt->at("VELPDX"));
                currentSpeed = static_cast<float>(sum / 2.0);
            } catch (const std::exception&) {
                std::cerr << "ERRORE [HallWindow]: Errore durante il calcolo della velocità." << std::endl;
            }
        }
    } else {
        // Modalità live
        std::lock_guard<std::mutex> lock(m_dataMutex);
        currentSpeed = m_currentSpeed;
    }

    // Pannello tachimetro 
    ImGui::BeginChild("TachimetroChild", ImVec2(0, m_speedometerPaneHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Tachimetro");
    ImGui::PopFont();
    
    ImGui::Separator();
    drawSpeedometer(currentSpeed);
    ImGui::EndChild();

    // Separatore trascinabile 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 0.25f));
    ImGui::Button("##splitter", ImVec2(-1, 8.0f));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsItemActive()) {
        m_speedometerPaneHeight += ImGui::GetIO().MouseDelta.y;
        if (m_speedometerPaneHeight < 150.0f) m_speedometerPaneHeight = 150.0f;
        float max_height = ImGui::GetWindowHeight() - 200.0f; 
        if (m_speedometerPaneHeight > max_height) m_speedometerPaneHeight = max_height;
    }
    ImGui::Spacing();
    ImGui::Spacing();

    // Pannello sensori hall
    ImGui::BeginChild("GraficoChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    
    // Calcolo dell'asse X e del Cursore
    double window_start_time, now_time;
    double cursor_time = 0.0; // Variabile per la linea verticale

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto currentRowOpt = ServiceManager::getPlaybackManager()->getCurrentRow();
        if (currentRowOpt) {
            auto center_ts = PacketParser::parseTimestampString(currentRowOpt->at("timestamp"));
            double centerTime = std::chrono::duration<double>(center_ts.time_since_epoch()).count();
            cursor_time = centerTime; // In playback il cursore è al centro
            window_start_time = centerTime - (MAX_HISTORY_SECONDS / 2.0);
            now_time = centerTime + (MAX_HISTORY_SECONDS / 2.0);
        } else {
            window_start_time = 0; now_time = 1;
        }
    } else {
        // Modalità live
        now_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        cursor_time = now_time; // In live il cursore è il tempo attuale
        window_start_time = now_time - 10;
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

    // Grafico sensori Hall
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Sensori effetto Hall");
    ImGui::PopFont();

    if (ImPlot::BeginPlot("##SensoriHall", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always);
        if (!tick_positions.empty()) ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");

        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            double min_y = std::numeric_limits<double>::max();
            double max_y = std::numeric_limits<double>::lowest();
            bool has_data = false;
            for (const auto& pair : m_plotData) {
                const auto& line_data = pair.second;
                 for (size_t i = 0; i < line_data.X.size(); ++i) {
                    if (line_data.X[i] >= window_start_time) {
                        if (line_data.Y[i] < min_y) min_y = line_data.Y[i];
                        if (line_data.Y[i] > max_y) max_y = line_data.Y[i];
                        has_data = true;
                    }
                }
            }
            if (has_data) {
                double final_min_y = 0.0, final_max_y = 120.0;
                final_min_y = (std::min)(final_min_y, min_y);
                final_max_y = (std::max)(final_max_y, max_y);
                double range = final_max_y - final_min_y;
                double padding = std::max(range * 0.10, 5.0); 
                ImPlot::SetupAxisLimits(ImAxis_Y1, final_min_y - padding, final_max_y + padding, ImGuiCond_Always);
            } else {
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 120.0, ImGuiCond_Always);
            }
            for (const auto& pair : m_plotData) {
                ImPlot::PlotLine(pair.first.c_str(), pair.second.X.data(), pair.second.Y.data(), pair.second.X.size());
            }
            
            // Disegna la linea rossa verticale al cursore temporale
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImPlot::PlotInfLines("##Cursor", &cursor_time, 1);
            ImPlot::PopStyleColor();
        }
        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
    ImGui::EndChild();
    ImGui::End();
}