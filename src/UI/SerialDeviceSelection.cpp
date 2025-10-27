#include <imgui.h>
#include <string> 
#include "UI/SerialDeviceSelection.hpp"
#include "Telemetry/Services/ServiceManager.hpp" 
#include "Telemetry/data_acquisition/SerialDataSource.hpp"
#include "UI/UIManager.hpp" 

SerialDeviceSelection::SerialDeviceSelection(UiManager* manager, ConnectCallback onConnectCallback)
    : m_uiManager(manager), m_onConnectCallback(std::move(onConnectCallback)) {
    refreshPorts();
}

void SerialDeviceSelection::draw() {
    ImGui::PushFont(m_uiManager->font_title);
    ImGui::Begin("Configurazione connessione");
    ImGui::PopFont();

   ImGui::Text("Porta Seriale");
    const char* current_port_label = (m_selectedPortIndex != -1) ? m_ports[m_selectedPortIndex].c_str() : "Seleziona una porta.";
    if (ImGui::BeginCombo("##Porta seriale", current_port_label)) {
        for (int i = 0; i < m_ports.size(); ++i) {
            if (ImGui::Selectable(m_ports[i].c_str(), m_selectedPortIndex == i)) {
                m_selectedPortIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Text("Baud Rate");
    const char* current_baud_label = (m_selectedBaudrateIndex != -1) ? std::to_string(m_baudRates[m_selectedBaudrateIndex]).c_str() : "Seleziona un baudrate.";
    if (ImGui::BeginCombo("##Baud rate", current_baud_label)) {
        for (int i = 0; i < m_baudRates.size(); ++i) {
            if (ImGui::Selectable(std::to_string(m_baudRates[i]).c_str(), m_selectedBaudrateIndex == i)) {
                m_selectedBaudrateIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (ImGui::Button("Aggiorna porte")) {
        refreshPorts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Connetti")) {
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

    SerialDataSource sds_temp; 
    m_baudRates = sds_temp.supportedBaudRates();

    m_selectedPortIndex = m_ports.empty() ? -1 : 0;

    m_selectedBaudrateIndex = -1;
    for (int i = 0; i < m_baudRates.size(); ++i) {
        if (m_baudRates[i] == 115200) {
            m_selectedBaudrateIndex = i;
            break;
        }
    }
    if (m_selectedBaudrateIndex == -1 && !m_baudRates.empty()) {
        m_selectedBaudrateIndex = 0;
    }
}

