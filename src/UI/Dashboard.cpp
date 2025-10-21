#include <imgui.h>
#include <variant>
#include <string>
#include "UI/Dashboard.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

Dashboard::Dashboard() = default;

void Dashboard::onDataReceived(const PacketParser& packet) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData[packet.label] = packet.data;
}

bool Dashboard::createFile(const std::string& directoryPath) {
    (void)directoryPath; 
    return true;
}

void Dashboard::draw() {
    ImGui::Begin("Dashboard Telemetria");

    ImGui::BeginChild("ControlPanel", ImVec2(0, 80), true);
    ImGui::Text("Controllo Registrazione");
    ImGui::Separator();
    
    bool isLogging = ServiceManager::isLogging();

    if (!isLogging) {
        if (ImGui::Button("Avvia Registrazione")) {
            ServiceManager::startLogging(OUTPUT_DIRECTORY);
        }
    } else {
        std::string currentLogFile = ServiceManager::getCurrentLogFileName();
        ImGui::Text("Registrazione in corso su: %s", currentLogFile.c_str());
        if (ImGui::Button("Ferma Registrazione")) {
            ServiceManager::stopLogging();
        }
    }
    ImGui::EndChild();

    ImGui::Text("Dati in Tempo Reale");
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Sensore/Stato");
        ImGui::TableSetupColumn("Valore Attuale");
        ImGui::TableHeadersRow();

        std::map<std::string, PacketData> dataCopy;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            dataCopy = m_latestData;
        }

        for (const auto& [label, data] : dataCopy) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label.c_str());
            ImGui::TableSetColumnIndex(1);
            
            std::string valueStr = std::visit([](auto&& arg) -> std::string {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) return arg;
                else return std::to_string(arg);
            }, data);
            
            ImGui::TextUnformatted(valueStr.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}