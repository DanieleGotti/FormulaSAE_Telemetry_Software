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

void InverterDataWindow::draw() {
    ImGui::Begin("Internal inverter data");

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

        printValue("Left", "inv_L_tempMotor", dataToDisplay, "°C");
        printValue("Right", "inv_R_tempMotor", dataToDisplay, "°C"); 

    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}