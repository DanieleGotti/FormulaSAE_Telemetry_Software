#include "UI/SerialDeviceSelection.hpp"
#include <imgui.h>

void SerialDeviceSelection::draw() {
    ImGui::Begin("Connessione seriale");

    if (ImGui::BeginTable("SerialConfig", 2, ImGuiTableFlags_SizingStretchProp)) {
        // --- Riga: Porte ---
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Porte");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1); // occupa tutta la colonna
        if (ImGui::BeginCombo("##Porte", m_selectedPort.empty() ? "Nessuna" : m_selectedPort.c_str())) {
            for (int n = 0; n < m_ports.size(); n++) {
                bool is_selected = (m_selectedIndex == n);
                if (ImGui::Selectable(m_ports[n].c_str(), is_selected)) {
                    m_selectedIndex = n;
                    m_selectedPort = m_ports[n];
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- Riga: Baud rate ---
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Baud rate");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);
        if (m_selectedIndex >= 0) {
            auto rates = m_serial.supportedBaudRates();
            if (ImGui::BeginCombo("##Baud", sstr(m_selectedBaudrate).c_str())) {
                for (int n = 0; n < rates.size(); n++) {
                    bool is_selected = (m_selectedBaudrate == rates[n]);
                    if (ImGui::Selectable(sstr(rates[n]).c_str(), is_selected)) {
                        m_selectedBaudrate = rates[n];
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextUnformatted("Nessuna porta disponibile");
        }

        ImGui::EndTable();
    }

    // --- Bottoni ---
    if (ImGui::Button("Aggiorna porte")) {
        refreshPorts();
    }
    ImGui::SameLine();
    if (!m_connected) {
        if (ImGui::Button("Connetti")) {
            if (m_serial.open(m_selectedPort, 115200)) {
                m_connected = true;
                appendLog("Connesso a " + m_selectedPort);
            } else {
                appendLog("Errore apertura porta " + m_selectedPort);
            }
        }
    } else {
        if (ImGui::Button("Disconnetti")) {
            m_serial.close();
            m_connected = false;
            appendLog("Disconnesso");
        }
    }

    // --- Log area ---
    ImGui::Separator();
    ImGui::Text("Log:");
    ImGui::BeginChild("ScrollingRegion", ImVec2(0,0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(m_log.c_str());
    ImGui::EndChild();

    ImGui::End();
}

void SerialDeviceSelection::refreshPorts() {
    std::string prevPort = m_selectedPort;
    m_ports = m_serial.getAvailableResources();
    if (!m_ports.empty()) {
        m_selectedPort = m_ports[0];
    } else {
        m_selectedPort.clear();
    }
    m_selectedIndex = 0;
    if (!m_ports.empty()) {
        m_selectedPort = m_ports[0];
    } else {
        m_selectedPort.clear();
    }
}

void SerialDeviceSelection::appendLog(const std::string& msg) {
    m_log += msg + "\n";
}