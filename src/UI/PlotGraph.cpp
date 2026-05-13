#include <cmath>
#include <limits>
#include <algorithm>
#include <map>
#include "UI/PlotGraph.hpp"

PlotGraph::PlotGraph(const std::string& title, const std::vector<std::pair<std::string, std::string>>& keysAndLabels, double minY, double maxY)
    : m_title(title), m_keysAndLabels(keysAndLabels), m_defaultMinY(minY), m_defaultMaxY(maxY) {}

int PlotGraph::TimeAxisFormatter(double value, char* buff, int size, void* user_data) {
    // 2 decimals added to display sub-second intervals when zoomed in
    return snprintf(buff, size, "%.2f s", value); 
}

void PlotGraph::draw(const std::map<std::string, PlotLineData>& plotData, double cursorTime, bool isPlayback, float height) {
    
    // We use static maps to remember the view state of each graph (distinguished by m_title)
    static std::map<std::string, double> s_last_cursor;
    static std::map<std::string, double> s_view_start;
    static std::map<std::string, double> s_view_end;

    // Initialize bounds if it's the first time running
    if (s_view_start.find(m_title) == s_view_start.end()) {
        s_view_start[m_title] = cursorTime - 5.0;
        s_view_end[m_title]   = cursorTime + 5.0;
    }

    // Check if the user is scrubbing the timeline slider
    bool cursor_changed = std::abs(cursorTime - s_last_cursor[m_title]) > 1e-5;
    s_last_cursor[m_title] = cursorTime;

    // Retrieve previous frame's X-axis bounds
    double view_start = s_view_start[m_title];
    double view_end = s_view_end[m_title];
    double current_width = view_end - view_start;
    
    if (current_width <= 0.1) current_width = 10.0; 

    if (!isPlayback) {
        // Live mode: force a trailing 10s window aligned to the right
        view_start = std::max(0.0, cursorTime - 10.0);
        view_end = cursorTime;
    } else if (cursor_changed) {
        // Playbackmode: move the window keeping the current zoom level
        view_start = cursorTime - (current_width * 0.5);
        view_end = cursorTime + (current_width * 0.5);
    }

    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    bool has_data = false;

    for (const auto& pair : m_keysAndLabels) {
        auto it = plotData.find(pair.first); 
        if (it != plotData.end()) {
            const auto& line = it->second;
            for (size_t i = 0; i < line.X.size(); ++i) {
                // Only consider data within the current X-axis zoom view!
                if (line.X[i] >= view_start && line.X[i] <= view_end) {
                    if (line.Y[i] < min_y) min_y = line.Y[i];
                    if (line.Y[i] > max_y) max_y = line.Y[i];
                    has_data = true;
                }
            }
        }
    }

    double final_min_y = m_defaultMinY;
    double final_max_y = m_defaultMaxY;

    if (has_data) {
        double range = max_y - min_y;
        if (range < 1e-5) range = 10.0;
        double padding = std::max(range * 0.10, 2.0);
        final_min_y = min_y - padding;
        final_max_y = max_y + padding;
    }

    ImPlotAxisFlags x_flags = ImPlotAxisFlags_NoHighlight;
    if (!isPlayback) x_flags |= ImPlotAxisFlags_Lock; // Disable user zoom in live mode

    if (ImPlot::BeginPlot(("##" + m_title).c_str(), ImVec2(-1, height), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        
        ImPlot::SetupAxes(nullptr, nullptr, x_flags, ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisFormat(ImAxis_X1, TimeAxisFormatter, nullptr);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");

        // Set the dynamically calculated Y limits
        ImPlot::SetupAxisLimits(ImAxis_Y1, final_min_y, final_max_y, ImGuiCond_Always);

        // ImGuiCond_Once allows the user to freely zoom with the mouse in Playback mode
        ImGuiCond x_cond = (cursor_changed || !isPlayback) ? ImGuiCond_Always : ImGuiCond_Once;
        ImPlot::SetupAxisLimits(ImAxis_X1, view_start, view_end, x_cond);

        // Plot lines
        for (const auto& pair : m_keysAndLabels) {
            auto it = plotData.find(pair.first);
            if (it != plotData.end() && !it->second.X.empty()) {
                ImPlot::PlotLine(pair.second.c_str(), it->second.X.data(), it->second.Y.data(), it->second.X.size());
            }
        }

        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImPlot::PlotInfLines("##Cursor", &cursorTime, 1);
        ImPlot::PopStyleColor();

        // Read the limits after plotting to capture user's mouse zoom/pan interactions
        ImPlotRect limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
        double new_width = limits.X.Max - limits.X.Min;
        
        // Prevent zooming out too much (max 20 seconds) to avoid freezing the UI 
        if (new_width > 20.0) {
            double center = (limits.X.Max + limits.X.Min) * 0.5;
            s_view_start[m_title] = center - 10.0;
            s_view_end[m_title]   = center + 10.0;
        } else {
            s_view_start[m_title] = limits.X.Min;
            s_view_end[m_title]   = limits.X.Max;
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}