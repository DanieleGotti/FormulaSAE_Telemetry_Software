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

    std::string valStr = "Null";
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        valStr = it->second;
    }

    std::string unitStr = unit.empty() ? "[-]" : "[" + unit + "]";

    ImGui::PushFont(m_uiManager->font_body);
    float valWidth = ImGui::CalcTextSize(valStr.c_str()).x;
    float unitWidth = ImGui::CalcTextSize(unitStr.c_str()).x;
    ImGui::PopFont();

    float spacing = 5.0f;
    float rightEdge = ImGui::GetWindowContentRegionMax().x;

    ImGui::SameLine(rightEdge - unitWidth - spacing - valWidth);
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::Text("%s", valStr.c_str());
    ImGui::PopFont();

    ImGui::SameLine(rightEdge - unitWidth);
    ImGui::PushFont(m_uiManager->font_body);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", unitStr.c_str());
    ImGui::PopFont();
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
        ImGui::Text("Waiting for data.");
    }

    ImGui::End();
}