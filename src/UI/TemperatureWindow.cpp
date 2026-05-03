#include <imgui.h>
#include <iostream>
#include <string>
#include <stdexcept> 
#include "UI/UIManager.hpp"
#include "UI/TemperatureWindow.hpp" 
#include "Telemetry/Services/ServiceManager.hpp"

TemperatureWindow::TemperatureWindow(UiManager* manager) : m_uiManager(manager) {}

void TemperatureWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

ImVec4 TemperatureWindow::getColorForThreshold(float value, float limitGreen, float limitRed) {
    if (value <= limitGreen) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f);       // Verde scuro
    else if (value >= limitRed) return ImVec4(0.85f, 0.1f, 0.1f, 1.0f);   // Rosso scuro
    else return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);                           // Arancione
}

void TemperatureWindow::printColoredValue(const std::string& label, const std::string& key, const DbRow& dataMap, float limitGreen, float limitRed) {
    ImGui::Text("%s:", label.c_str());

    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        std::string strVal = it->second;
        try {
            float val = std::stof(strVal);
            ImVec4 color = getColorForThreshold(val, limitGreen, limitRed);
            
            ImGui::PushFont(m_uiManager->font_body);
            float text_width = ImGui::CalcTextSize(strVal.c_str()).x;
            ImGui::SameLine(140.0f - text_width); // Stesso offset per allineamento UI perfetto
            ImGui::TextColored(color, "%s", strVal.c_str());
            ImGui::PopFont();

            ImGui::SameLine(145.0f);
            ImGui::PushFont(m_uiManager->font_body);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[°C]");
            ImGui::PopFont();
            
            if (val >= limitRed && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("ATTENZIONE: Valore critico");
            }
        } catch (const std::invalid_argument&) {
            ImGui::PushFont(m_uiManager->font_body);
            float w = ImGui::CalcTextSize("NaN").x;
            ImGui::SameLine(140.0f - w);
            ImGui::Text("NaN");
            ImGui::PopFont();
        }
    } else {
        ImGui::PushFont(m_uiManager->font_body);
        float w = ImGui::CalcTextSize("N/D").x;
        ImGui::SameLine(140.0f - w);
        ImGui::Text("N/D");
        ImGui::PopFont();
    }
}

void TemperatureWindow::draw() {
    ImGui::Begin("External thermal data");

    DbRow dataToDisplay;
    bool hasData = false;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto row = playbackManager->getCurrentRow()) {
            dataToDisplay = *row;
            hasData = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_latestData.empty()) {
            dataToDisplay = m_latestData;
            hasData = true;
        }
    }

    if (hasData) {
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Coolant pump temperatures");
        ImGui::PopFont();
        
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushFont(m_uiManager->font_body);
        printColoredValue("Left", "left_coolant_temp", dataToDisplay, 60.0f, 80.0f);
        printColoredValue("Right", "right_coolant_temp", dataToDisplay, 60.0f, 80.0f);
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Motor temperatures");
        ImGui::PopFont();
        
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushFont(m_uiManager->font_body);
        printColoredValue("Left", "left_motor_temp", dataToDisplay, 100.0f, 120.0f);
        printColoredValue("Right", "right_motor_temp", dataToDisplay, 100.0f,120.0f);
        ImGui::PopFont();

    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}