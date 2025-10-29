#include <imgui.h>
#include <implot.h> 
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include "UI/AccBrkWindow.hpp"
#include "UI/UIManager.hpp"

struct TimeFormatterData {
    // La durata totale in secondi dell'asse X visibile.
    double AxisSpan; 
};

static int TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    TimeFormatterData* data = static_cast<TimeFormatterData*>(user_data);
    time_t time = static_cast<time_t>(value);
    
#ifdef _WIN32
    tm tm_info;
    localtime_s(&tm_info, &time);
#else
    // Per sistemi non-Windows
    tm tm_info = *std::localtime(&time);
#endif

    // Se la finestra visibile è meno di 2 secondi, mostriamo i decimi altrimenti solo i secondi
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


AccBrkWindow::AccBrkWindow(UiManager* manager) : m_uiManager(manager) {
    // Per i sensori da plottare
    const std::vector<std::string> keys_to_plot = {
        "ACC1A", "ACC1B", "ACC2A", "ACC2B", "BRK1", "BRK2"
    };
    for (const auto& key : keys_to_plot) {
        m_plotData[key] = PlotLineData();
    }
}

std::chrono::system_clock::time_point AccBrkWindow::parseTimestamp(const std::string& ts_str) {
    std::tm tm = {};
    std::stringstream ss(ts_str.substr(0, 19)); // "YYYY-MM-DDTHH:MM:SS"
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    int ms = std::stoi(ts_str.substr(20, 3));
    time_point += std::chrono::milliseconds(ms);
    
    return time_point;
}

void AccBrkWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        auto current_timestamp = parseTimestamp(dataRow.at("timestamp"));
        // Converte il time_point in un timestamp UNIX (come double)
        auto duration = current_timestamp.time_since_epoch();
        double unix_timestamp = std::chrono::duration<double>(duration).count();

        for (auto& pair : m_plotData) {
            const std::string& label = pair.first;
            PlotLineData& line_data = pair.second;

            if (dataRow.count(label)) {
                // Salva il timestamp UNIX sull'asse x
                line_data.X.push_back(unix_timestamp);
                line_data.Y.push_back(std::stod(dataRow.at(label)));
                // La logica per pulire i dati vecchi rimane utile per la memoria
                while (!line_data.X.empty() && line_data.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    line_data.X.erase(line_data.X.begin());
                    line_data.Y.erase(line_data.Y.begin());
                }
            }
        }
    } catch (const std::exception& e) {
        // Ignora
    }
}

void AccBrkWindow::draw() {
    ImGui::Begin("Acceleratori e freni", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    float available_height = ImGui::GetContentRegionAvail().y;
    float plot_height = (available_height - ImGui::GetStyle().ItemSpacing.y * 3 - ImGui::GetTextLineHeightWithSpacing()) / 2.0f;
    if (plot_height < 100) plot_height = 100;
    
    ImPlotAxisFlags x_flags = ImPlotAxisFlags_NoHighlight;
    ImPlotAxisFlags y_flags = ImPlotAxisFlags_NoHighlight; 

    double now_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    double window_start_time = now_time - 10;

    // La logica per i tick dell'asse x 
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

    if (chosen_step < 1.0)
        chosen_step = 1.0;

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

    // Grafico acceleratori
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Acceleratori");
    ImGui::PopFont();
    
    if (ImPlot::BeginPlot("##Acceleratori", ImVec2(-1, plot_height), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, x_flags, y_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always);

        if (!tick_positions.empty())
            ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr); 
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");
        
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            
            // Calcola limiti y 
            double min_y = std::numeric_limits<double>::max();
            double max_y = std::numeric_limits<double>::lowest();
            bool has_data = false;

            const std::vector<std::string> acc_keys = {"ACC1A", "ACC1B", "ACC2A", "ACC2B"};
            for (const auto& key : acc_keys) {
                const auto& line_data = m_plotData[key];
                for (size_t i = 0; i < line_data.X.size(); ++i) {
                    if (line_data.X[i] >= window_start_time) {
                        if (line_data.Y[i] < min_y) min_y = line_data.Y[i];
                        if (line_data.Y[i] > max_y) max_y = line_data.Y[i];
                        has_data = true;
                    }
                }
            }

            if (has_data) {
                if (std::abs(max_y - min_y) < 1.0) {
                     max_y += 5.0; // Aggiunge un piccolo margine
                }
                double padding = (max_y - min_y) * 0.10; // Margine del 10% 
                ImPlot::SetupAxisLimits(ImAxis_Y1, min_y - padding, max_y + padding, ImGuiCond_Always);
            } else {
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
            }

            ImPlot::PlotLine("ACC1A", m_plotData["ACC1A"].X.data(), m_plotData["ACC1A"].Y.data(), m_plotData["ACC1A"].X.size());
            ImPlot::PlotLine("ACC1B", m_plotData["ACC1B"].X.data(), m_plotData["ACC1B"].Y.data(), m_plotData["ACC1B"].X.size());
            ImPlot::PlotLine("ACC2A", m_plotData["ACC2A"].X.data(), m_plotData["ACC2A"].Y.data(), m_plotData["ACC2A"].X.size());
            ImPlot::PlotLine("ACC2B", m_plotData["ACC2B"].X.data(), m_plotData["ACC2B"].Y.data(), m_plotData["ACC2B"].X.size());
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }

    ImGui::Spacing();

    // Grafico freni
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Freni");
    ImGui::PopFont();
    
    if (ImPlot::BeginPlot("##Freni", ImVec2(-1, plot_height), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, x_flags, y_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always);

        if (!tick_positions.empty())
            ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);

        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");

        {
            std::lock_guard<std::mutex> lock(m_dataMutex);

            // Calcola limiti y
            double min_y = std::numeric_limits<double>::max();
            double max_y = std::numeric_limits<double>::lowest();
            bool has_data = false;

            const std::vector<std::string> brk_keys = {"BRK1", "BRK2"};
            for (const auto& key : brk_keys) {
                const auto& line_data = m_plotData[key];
                 for (size_t i = 0; i < line_data.X.size(); ++i) {
                    if (line_data.X[i] >= window_start_time) {
                        if (line_data.Y[i] < min_y) min_y = line_data.Y[i];
                        if (line_data.Y[i] > max_y) max_y = line_data.Y[i];
                        has_data = true;
                    }
                }
            }

            if (has_data) {
                if (std::abs(max_y - min_y) < 1.0) {
                     max_y += 5.0;
                }
                double padding = (max_y - min_y) * 0.10; // Margine del 10%
                ImPlot::SetupAxisLimits(ImAxis_Y1, min_y - padding, max_y + padding, ImGuiCond_Always);
            } else {
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
            }

            ImPlot::PlotLine("BRK1", m_plotData["BRK1"].X.data(), m_plotData["BRK1"].Y.data(), m_plotData["BRK1"].X.size());
            ImPlot::PlotLine("BRK2", m_plotData["BRK2"].X.data(), m_plotData["BRK2"].Y.data(), m_plotData["BRK2"].X.size());
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }

    ImGui::End();
}