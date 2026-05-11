#include <imgui.h>
#include <iostream>
#include <string>
#include <stdexcept> 
#include "UI/UIManager.hpp"
#include "UI/InverterDataWindow.hpp" 
#include "Telemetry/Services/ServiceManager.hpp"

InverterDataWindow::InverterDataWindow(UiManager* manager) : m_uiManager(manager) {}

void InverterDataWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

void InverterDataWindow::printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit) {
    ImGui::Text("%s:", label.c_str());

    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        ImGui::PushFont(m_uiManager->font_body); 
        float text_width = ImGui::CalcTextSize(it->second.c_str()).x;
        ImGui::SameLine(140.0f - text_width); // Stesso offset di Dynamics per coerenza visiva
        
        ImGui::Text("%s", it->second.c_str()); 
        ImGui::PopFont();

        ImGui::SameLine(145.0f); 
        ImGui::PushFont(m_uiManager->font_body); 
        if (!unit.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", unit.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[-]");
        }
        ImGui::PopFont();
    } else {
        ImGui::PushFont(m_uiManager->font_body);
        float text_width = ImGui::CalcTextSize("N/D").x;
        ImGui::SameLine(140.0f - text_width);
        ImGui::Text("N/D");
        ImGui::PopFont();
    }
}

ImVec4 InverterDataWindow::getColorForThreshold(float value, float limitGreen, float limitRed) {
    if (value <= limitGreen) return ImVec4(0.1f, 0.8f, 0.1f, 1.0f);       // Verde scuro
    else if (value >= limitRed) return ImVec4(0.85f, 0.1f, 0.1f, 1.0f);   // Rosso scuro
    else return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);                           // Arancione
}

void InverterDataWindow::printColoredValue(const std::string& label, const std::string& key, const DbRow& dataMap, float limitGreen, float limitRed) {
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

void InverterDataWindow::draw() {
    ImGui::Begin("Inverter and cooling data");

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
        ImGui::Text("Torque current");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printValue("Left", "inv_L_torqueCurrent", dataToDisplay, "A");
        printValue("Right", "inv_R_torqueCurrent", dataToDisplay, "A");

        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Magnetizing current");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();
        
        printValue("Left", "inv_L_magnetizingCurrent", dataToDisplay, "A");
        printValue("Right", "inv_R_magnetizingCurrent", dataToDisplay, "A");
        
        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Motor temperature");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printColoredValue("Left", "inv_L_tempMotor", dataToDisplay, 100.0f, 120.0f);
        printColoredValue("Right", "inv_R_tempMotor", dataToDisplay, 100.0f,120.0f);
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Cooling pumps temperature");
        ImGui::PopFont();
        ImGui::Separator();
        
        printColoredValue("Left", "left_coolant_temp", dataToDisplay, 100.0f, 120.0f);
        printColoredValue("Right", "right_coolant_temp", dataToDisplay, 100.0f,120.0f);
        
    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}