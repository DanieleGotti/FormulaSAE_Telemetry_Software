#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility> // Per std::pair
#include <imgui.h>
#include <implot.h>

struct PlotLineData {
    std::vector<double> X, Y;
};

class PlotGraph {
public:
    // keysAndLabels: { {"chiave_struttura", "Etichetta Leggenda"}, ... }
    PlotGraph(const std::string& title, const std::vector<std::pair<std::string, std::string>>& keysAndLabels, double minY = 0.0, double maxY = 100.0);
    
    void draw(const std::map<std::string, PlotLineData>& plotData, double cursorTime, bool isPlayback, float height = -1.0f);

private:
    std::string m_title;
    std::vector<std::pair<std::string, std::string>> m_keysAndLabels;
    double m_defaultMinY;
    double m_defaultMaxY;
    
    static int TimeAxisFormatter(double value, char* buff, int size, void* user_data);
};