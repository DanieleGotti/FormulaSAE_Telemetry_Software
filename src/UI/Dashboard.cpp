#include <imgui.h>
#include "UI/Dashboard.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include <vector>
#include <string>

Dashboard::Dashboard() = default;

void Dashboard::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

void Dashboard::draw() {
    ImGui::Begin("Dashboard Telemetria");

    // Pannello di controllo per avviare/fermare la registrazione
    ImGui::BeginChild("ControlPanel", ImVec2(0, 80), true);
    ImGui::Text("Controllo Registrazione");
    ImGui::Separator();
    
    bool isLogging = ServiceManager::isLogging();

    if (!isLogging) {
        if (ImGui::Button("Avvia Registrazione")) {
            ServiceManager::startLogging(OUTPUT_DIRECTORY);
        }
    } else {
        ImGui::Text("Registrazione in corso...");
        ImGui::Text("Salvataggio in: %s/", OUTPUT_DIRECTORY.c_str());
        
        if (ImGui::Button("Ferma Registrazione")) {
            ServiceManager::stopLogging();
        }
    }
    ImGui::EndChild();

    ImGui::Text("Dati in Tempo Reale (Aggregati per Timestamp)");
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Sensore/Stato");
        ImGui::TableSetupColumn("Valore Attuale");
        ImGui::TableHeadersRow();

        std::map<std::string, std::string> dataCopy;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            dataCopy = m_latestData;
        }

        for (const auto& [label, value] : dataCopy) {
            if (label == "timestamp") continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label.c_str());
            ImGui::TableSetColumnIndex(1);

            std::string displayValue = value;
            if (label == "LEFT_INVERTER_FSM" || label == "RIGHT_INVERTER_FSM") {
                const std::string separator = " / ";
                size_t pos = value.rfind(separator);
                if (pos != std::string::npos) {
                    displayValue = value.substr(pos + separator.length());
                }
            }
            ImGui::TextUnformatted(displayValue.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}