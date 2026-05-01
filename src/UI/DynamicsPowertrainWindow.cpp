#include <imgui.h>
#include <iostream>
#include <string>
#include <stdexcept> 
#include "UI/UIManager.hpp"
#include "UI/DynamicsPowertrainWindow.hpp" 
#include "Telemetry/Services/ServiceManager.hpp"

DynamicsPowertrainWindow::DynamicsPowertrainWindow(UiManager* manager) : m_uiManager(manager) {}

void DynamicsPowertrainWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

void DynamicsPowertrainWindow::printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit) {
    ImGui::Text("%s:", label.c_str());

    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        ImGui::PushFont(m_uiManager->font_label); // Valore in grassetto
        float text_width = ImGui::CalcTextSize(it->second.c_str()).x;
        ImGui::SameLine(190.0f - text_width); // Offset avvicinato
        
        ImGui::Text("%s", it->second.c_str()); // Colore default (si adatta al tema)
        ImGui::PopFont();

        ImGui::SameLine(195.0f); // Offset unità
        ImGui::PushFont(m_uiManager->font_body); 
        if (!unit.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", unit.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[-]");
        }
        ImGui::PopFont();
    } else {
        ImGui::PushFont(m_uiManager->font_label);
        float text_width = ImGui::CalcTextSize("N/D").x;
        ImGui::SameLine(190.0f - text_width);
        ImGui::Text("N/D");
        ImGui::PopFont();
    }
}

void DynamicsPowertrainWindow::draw() {
    ImGui::Begin("Vehicle dynamics and powertrain targets");

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
        ImGui::Text("Vehicle dynamics");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printValue("Mean velocity", "mean_velocity", dataToDisplay, "km/h");
        printValue("Yaw rate", "real_yaw_rate", dataToDisplay, "°/s");
        printValue("Total torque request", "total_torque_request", dataToDisplay, "Nm");

        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Tyre slips");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();
        
        printValue("Left", "slip_L", dataToDisplay, "");
        printValue("Right", "slip_R", dataToDisplay, "");
        
        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Torque vectoring");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printValue("Left", "torque_tv_L", dataToDisplay, "Nm");
        printValue("Right", "torque_tv_R", dataToDisplay, "Nm"); 

        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Traction control");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printValue("Left", "torque_reduction_L", dataToDisplay, "Nm");
        printValue("Right", "torque_reduction_R", dataToDisplay, "Nm"); 

        ImGui::Spacing();
        ImGui::Separator();
        
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Final torque targets");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        printValue("Left", "final_torque_target_L", dataToDisplay, "Nm");
        printValue("Right", "final_torque_target_R", dataToDisplay, "Nm"); 

    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}