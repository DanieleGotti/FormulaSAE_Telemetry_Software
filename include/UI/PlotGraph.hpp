/**
 * @file PlotGraph.hpp
 * @brief Reusable time plot (ImPlot) with multiple lines, cursor and automatic Y scale.
 *
 * @details
 * Used by the windows with plots (AccBrkWindow, HallWindow, SuspensionWindow,
 * TemperatureWindow). Behaviour:
 *  - **live**: fixed trailing window of the last 10 s aligned to the right, X zoom locked;
 *  - **playback**: the view follows the cursor keeping the zoom chosen with the
 *    mouse (at most 20 s wide);
 *  - the Y axis adapts to the visible data (+10% padding, at least 2 units).
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility> 
#include <imgui.h>
#include <implot.h>

/**
 * @brief Points of one line: X = time (s), Y = value.
 */
struct PlotLineData {
    std::vector<double> X, Y;
};

/**
 * @brief Plot with one or more lines read from a key -> PlotLineData map.
 */
class PlotGraph {
public:
    /**
     * @brief Creates the plot.
     * @param title         Title, also used as the unique plot ID (must differ for every plot).
     * @param keysAndLabels Lines to draw: { {"key in the map", "legend label"}, ... }.
     * @param minY          Lower Y axis limit when no data is visible.
     * @param maxY          Upper Y axis limit when no data is visible.
     */
    PlotGraph(const std::string& title, const std::vector<std::pair<std::string, std::string>>& keysAndLabels, double minY = 0.0, double maxY = 100.0);
    
    /**
     * @brief Draws the plot inside the current ImGui window.
     * @param plotData   Available series (keys that are not present are ignored).
     * @param cursorTime Time (s) of the red vertical cursor (latest sample in live,
     *                   slider position in playback).
     * @param isPlayback true in playback mode (free zoom/pan on the X axis).
     * @param height     Height in pixels; -1 = all the available space.
     */
    void draw(const std::map<std::string, PlotLineData>& plotData, double cursorTime, bool isPlayback, float height = -1.0f);

private:
    std::string m_title;                                         ///< Plot title/ID.
    std::vector<std::pair<std::string, std::string>> m_keysAndLabels; ///< Lines: data key -> label.
    double m_defaultMinY;                                        ///< Minimum Y without data.
    double m_defaultMaxY;                                        ///< Maximum Y without data.
    
    /** @brief X axis formatter: seconds with 2 decimals (e.g. "12.35 s"). */
    static int TimeAxisFormatter(double value, char* buff, int size, void* user_data);
};