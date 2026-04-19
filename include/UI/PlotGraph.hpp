#pragma once
#include <string>
#include <vector>
#include <map>
#include <imgui.h>
#include <implot.h>

// Struttura dati base condivisa tra tutte le finestre grafiche
struct PlotLineData {
    std::vector<double> X, Y;
};

class PlotGraph {
public:
    PlotGraph(const std::string& title, const std::vector<std::string>& keys, double minY = 0.0, double maxY = 100.0);
    
    // Disegna il grafico passando i dati, il cursore temporale e la modalità
    void draw(const std::map<std::string, PlotLineData>& plotData, double cursorTime, bool isPlayback, float height = -1.0f);

private:
    std::string m_title;
    std::vector<std::string> m_keys;
    double m_defaultMinY;
    double m_defaultMaxY;
    
    static int TimeAxisFormatter(double value, char* buff, int size, void* user_data);
};