#include "UI/PlotGraph.hpp"
#include <cmath>
#include <limits>
#include <algorithm>

PlotGraph::PlotGraph(const std::string& title, const std::vector<std::string>& keys, double minY, double maxY)
    : m_title(title), m_keys(keys), m_defaultMinY(minY), m_defaultMaxY(maxY) {}

int PlotGraph::TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    return snprintf(buff, size, "%.2f s", value);
}

void PlotGraph::draw(const std::map<std::string, PlotLineData>& plotData, double cursorTime, bool isPlayback, float height) {
    double window_start_time, window_end_time;
    
    // Finestra di 10 secondi esatta
    if (isPlayback) {
        // Modalità playback: cursorTime è fisso al centro (-5s ... +5s)
        window_start_time = cursorTime - 5.0;
        window_end_time = cursorTime + 5.0;
    } else {
        // Modalità Live: cursorTime è l'ultimo pacchetto ricevuto e sta tutto a destra
        window_start_time = std::max(0.0, cursorTime - 10.0);
        window_end_time = cursorTime;
    }

    // Calcolo tick per l'asse X
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
    for (double sec = first_tick; sec <= window_end_time + chosen_step; sec += chosen_step) {
        if (label_idx >= 20) break;
        if (sec >= window_start_time) {
            tick_positions.push_back(sec);
            TimeAxisFormatter(sec, labels_buffer[label_idx], 32, nullptr);
            tick_labels.push_back(labels_buffer[label_idx]);
            label_idx++;
        }
    }

    if (ImPlot::BeginPlot(("##" + m_title).c_str(), ImVec2(-1, height), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, window_end_time, ImGuiCond_Always);
        if (!tick_positions.empty()) ImPlot::SetupAxisTicks(ImAxis_X1, tick_positions.data(), tick_positions.size(), tick_labels.data(), false);
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");

        // Autoscale intelligente per l'asse Y
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        bool has_data = false;
        
        for (const auto& key : m_keys) {
            auto it = plotData.find(key);
            if (it != plotData.end()) {
                const auto& line = it->second;
                for (size_t i = 0; i < line.X.size(); ++i) {
                    if (line.X[i] >= window_start_time && line.X[i] <= window_end_time) {
                        if (line.Y[i] < min_y) min_y = line.Y[i];
                        if (line.Y[i] > max_y) max_y = line.Y[i];
                        has_data = true;
                    }
                }
            }
        }

        if (has_data) {
            double range = max_y - min_y;
            if (range < 1e-5) range = 10.0; // previene asse piatto
            double padding = std::max(range * 0.10, 2.0);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_y - padding, max_y + padding, ImGuiCond_Always);
        } else {
            ImPlot::SetupAxisLimits(ImAxis_Y1, m_defaultMinY, m_defaultMaxY, ImGuiCond_Always);
        }

        // Plotta i dati effettivi
        for (const auto& key : m_keys) {
            auto it = plotData.find(key);
            if (it != plotData.end() && !it->second.X.empty()) {
                ImPlot::PlotLine(key.c_str(), it->second.X.data(), it->second.Y.data(), it->second.X.size());
            }
        }

        // Cursore/Linea rossa
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImPlot::PlotInfLines("##Cursor", &cursorTime, 1);
        ImPlot::PopStyleColor();

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}