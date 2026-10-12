/**
 * @file SerialDeviceSelection.cpp
 * @brief Implementation of the serial configuration window (see SerialDeviceSelection.hpp).
 */
#include <imgui.h>
#include <string> 
#include "UI/UIManager.hpp" 
#include "UI/SerialDeviceSelection.hpp"
#include "Telemetry/Services/ServiceManager.hpp" 
#include "Telemetry/data_acquisition/SerialDataSource.hpp"

SerialDeviceSelection::SerialDeviceSelection(UiManager* manager, ConnectCallback onConnectCallback)
    : m_uiManager(manager), m_onConnectCallback(std::move(onConnectCallback)) {
    refreshPorts();
}

void SerialDeviceSelection::draw() {
    ImGui::PushFont(m_uiManager->font_title);
    ImGui::Begin("Configure serial connection");
    ImGui::PopFont();

    // ---- Port drop-down ----
   ImGui::Text("Serial port");
    const char* current_port_label = (m_selectedPortIndex != -1) ? m_ports[m_selectedPortIndex].c_str() : "Select a port";
    if (ImGui::BeginCombo("##Serial port", current_port_label)) {
        for (int i = 0; i < m_ports.size(); ++i) {
            if (ImGui::Selectable(m_ports[i].c_str(), m_selectedPortIndex == i)) {
                m_selectedPortIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    // ---- Baud rate drop-down ----
    ImGui::Text("Baud rate");
    const char* current_baud_label = (m_selectedBaudrateIndex != -1) ? std::to_string(m_baudRates[m_selectedBaudrateIndex]).c_str() : "Select a baud rate";
    if (ImGui::BeginCombo("##Baud rate", current_baud_label)) {
        for (int i = 0; i < m_baudRates.size(); ++i) {
            if (ImGui::Selectable(std::to_string(m_baudRates[i]).c_str(), m_selectedBaudrateIndex == i)) {
                m_selectedBaudrateIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // ---- Buttons ----
    if (ImGui::Button("Refresh ports")) {
        refreshPorts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Connect")) {
        // Connect only if both port and baud rate are selected
        if (m_onConnectCallback && m_selectedPortIndex != -1 && m_selectedBaudrateIndex != -1) {
            const std::string& port = m_ports[m_selectedPortIndex];
            const int baudrate = m_baudRates[m_selectedBaudrateIndex];
            m_onConnectCallback(port, baudrate);
        }
    }

    ImGui::End();
}

void SerialDeviceSelection::refreshPorts() {
    m_ports = ServiceManager::getAllConnectionOptions();

    // Temporary instance used only to get the supported baud rates
    SerialDataSource sds_temp; 
    m_baudRates = sds_temp.supportedBaudRates();

    m_selectedPortIndex = m_ports.empty() ? -1 : 0;

    // Default baud rate: 500000 (the one used by the ECU), otherwise the first in the list
    m_selectedBaudrateIndex = -1;
    for (int i = 0; i < m_baudRates.size(); ++i) {
        if (m_baudRates[i] == 500000) {
            m_selectedBaudrateIndex = i;
            break;
        }
    }
    if (m_selectedBaudrateIndex == -1 && !m_baudRates.empty()) {
        m_selectedBaudrateIndex = 0;
    }
}
