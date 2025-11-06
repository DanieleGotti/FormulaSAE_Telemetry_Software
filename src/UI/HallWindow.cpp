#include <imgui.h>
#include <implot.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <numeric> // Per std::accumulate
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
    // Inizializza le chiavi per i 4 sensori di Hall
    const std::vector<std::string> keys_to_plot = {
        "VELADX", "VELASX", "VELPSX", "VELPDX" // Esempio di nomi, sostituisci con quelli reali
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
    } catch (...) {
        // Ignora eccezioni di parsing
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
                
                // Rimuovi dati vecchi per ottimizzare la memoria
                while (!line_data.X.empty() && line_data.X.front() < unix_timestamp - MAX_HISTORY_SECONDS) {
                    line_data.X.erase(line_data.X.begin());
                    line_data.Y.erase(line_data.Y.begin());
                }
            }
        }
        
        // Aggiorna la velocità media se abbiamo ricevuto dati validi
        if (count > 0) {
            m_currentSpeed = static_cast<float>(sum / count);
        }

    } catch (const std::exception& e) {
        // Gestisci o ignora errori di parsing/accesso
    }
}

void HallWindow::drawSpeedometer(float speed) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    float window_width = ImGui::GetContentRegionAvail().x;
    float radius = window_width / 4.0f;
    if (radius < 50) radius = 50;
    center.x += window_width / 2.0f;
    center.y += radius + 20;

    // Angoli per il semicerchio (da -90 a 90 gradi)
    float start_angle = (float)M_PI;
    float end_angle = 2.0f * (float)M_PI;
    float speed_normalized = std::fmax(0.0f, std::fmin(300.0f, speed)) / 300.0f;
    float speed_angle = start_angle + speed_normalized * (float)M_PI;

    // Sfondo del tachimetro
    draw_list->PathClear();
    for (int i = 0; i <= 20; ++i) {
        float angle = start_angle + ((float)i / 20.0f) * ((float)M_PI);
        draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
    }
    draw_list->PathStroke(ImColor(100, 100, 100), false, 3.0f);

    // Indicatore della velocità attuale
    draw_list->PathClear();
    for (int i = 0; i <= (int)(speed_normalized * 20); ++i) {
        float angle = start_angle + ((float)i / 20.0f) * ((float)M_PI);
        draw_list->PathLineTo(ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius));
    }
    draw_list->PathStroke(ImColor(255, 165, 0), false, 5.0f);

    // Lancetta
    draw_list->AddLine(
        center,
        ImVec2(center.x + cos(speed_angle) * (radius - 10), center.y + sin(speed_angle) * (radius - 10)),
        ImColor(255, 0, 0),
        3.0f
    );
    
    // Testo della velocità
    ImGui::PushFont(m_uiManager->font_data);
    char speed_text[16];
    snprintf(speed_text, 16, "%.0f", speed);
    ImVec2 text_size = ImGui::CalcTextSize(speed_text);
    ImGui::SetCursorScreenPos(ImVec2(center.x - text_size.x * 0.5f, center.y - text_size.y));
    ImGui::Text("%s", speed_text);
    ImGui::PopFont();
    
    ImGui::PushFont(m_uiManager->font_label);
    const char* unit_text = "km/h";
    ImVec2 unit_text_size = ImGui::CalcTextSize(unit_text);
    ImGui::SetCursorScreenPos(ImVec2(center.x - unit_text_size.x * 0.5f, center.y + 5));
    ImGui::Text("%s", unit_text);
    ImGui::PopFont();

    // Spaziatore per lasciare l'area di disegno libera
    ImGui::Dummy(ImVec2(window_width, radius * 1.5f));
}


void HallWindow::draw() {
    ImGui::Begin("Sensori Hall", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // --- Pannello Superiore (Tachimetro) ---
    ImGui::BeginChild("SpeedometerChild", ImVec2(0, m_speedometerPaneHeight), false, ImGuiWindowFlags_NoScrollbar);
    drawSpeedometer(m_currentSpeed);
    ImGui::EndChild();

    // --- Separatore Trascinabile ---
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Rimuove lo spazio extra
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Pulsante trasparente
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 0.25f));

    ImGui::Button("##splitter", ImVec2(-1, 8.0f)); // Il nostro splitter

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive()) {
        m_speedometerPaneHeight += ImGui::GetIO().MouseDelta.y;
        
        // Imposta dei limiti per evitare che i pannelli diventino troppo piccoli
        if (m_speedometerPaneHeight < 150.0f) {
            m_speedometerPaneHeight = 150.0f;
        }
        float max_height = ImGui::GetWindowHeight() - 200.0f; // Assicura che il grafico abbia almeno 200px
        if (m_speedometerPaneHeight > max_height) {
            m_speedometerPaneHeight = max_height;
        }
    }


    // --- Pannello Inferiore (Grafico) ---
    ImGui::BeginChild("PlotChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    
    double now_time = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    double window_start_time = now_time - 10;
    
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Segnali Sensori Hall");
    ImGui::PopFont();

    // Usa ImVec2(-1, -1) per far sì che il grafico riempia tutto lo spazio del pannello figlio
    if (ImPlot::BeginPlot("##HallSensors", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoHighlight, ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_X1, window_start_time, now_time, ImGuiCond_Always);
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
                double padding = (max_y - min_y) * 0.10;
                if (padding < 5.0) padding = 5.0; // Margine minimo
                ImPlot::SetupAxisLimits(ImAxis_Y1, min_y - padding, max_y + padding, ImGuiCond_Always);
            } else {
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 300, ImGuiCond_Always); // Default a 0-300
            }
            
            // Plotta le 4 linee dei sensori
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