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

// Restituisce un colore basato sui limiti specificati
ImVec4 TemperatureWindow::getColorForThreshold(float value, float limitGreen, float limitRed) {
    if (value <= limitGreen) return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); 
    else if (value >= limitRed) return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); 
    else return ImVec4(1.0f, 0.64f, 0.0f, 1.0f); 
}

// Stampa un valore con colore basato sui limiti
void TemperatureWindow::printColoredValue(const std::string& label, const std::string& key, const DbRow& dataMap, float limitGreen, float limitRed) {
    ImGui::Text("%s: ", label.c_str());
    ImGui::SameLine();

    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        std::string strVal = it->second;
        try {
            float val = std::stof(strVal);
            ImVec4 color = getColorForThreshold(val, limitGreen, limitRed);
            ImGui::PushFont(m_uiManager->font_label);
            ImGui::TextColored(color, "%s", strVal.c_str());
            ImGui::SameLine();
            ImGui::Text("°C");
            ImGui::PopFont();
            
            if (val >= limitRed && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("ATTENZIONE: Valore critico");
            }
        } catch (const std::invalid_argument&) {
            std::cerr << "ERRORE [TemperatureWindow]: Errore di conversione della temperatura." << std::endl;
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "N/D");
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
        ImGui::PushFont(m_uiManager->font_body);
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

        ImGui::PushFont(m_uiManager->font_body);
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