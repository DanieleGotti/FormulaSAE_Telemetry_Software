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
    ImGui::Begin("Telemetria");

    // Pannello di controllo per avviare/fermare la registrazione
    ImGui::BeginChild("ControlPanel", ImVec2(0, 80), true);
    ImGui::Text("Controllo registrazione");
    ImGui::Separator();
    
    bool isLogging = ServiceManager::isLogging();

    if (!isLogging) {
        if (ImGui::Button("Avvia registrazione")) {
            ServiceManager::startLogging(OUTPUT_DIRECTORY);
        }
    } else {
        ImGui::Text("Regitrazione in corso. Salvataggio in: %s.", OUTPUT_DIRECTORY.c_str());
        
        if (ImGui::Button("Ferma registrazione")) {
            ServiceManager::stopLogging();
        }
    }
    ImGui::EndChild();

    ImGui::Text("Dati");
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Tipo");
        ImGui::TableSetupColumn("Valore");
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