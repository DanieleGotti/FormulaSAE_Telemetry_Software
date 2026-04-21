#include <imgui.h>
#include <vector>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/Dashboard.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"

Dashboard::Dashboard(UiManager* manager) : m_uiManager(manager) {}

void Dashboard::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

void Dashboard::draw() {
    ImGui::Begin("Data management");

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_LIVE) {
        ImGui::BeginChild("ControlPanel", ImVec2(0, 100), true);
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Logging control");
        ImGui::PopFont();
        ImGui::Separator();
        
        bool isLogging = ServiceManager::isLogging();
        if (!isLogging) {
            if (ImGui::Button("Start logging")) {
                ServiceManager::startLogging(OUTPUT_DIRECTORY);
            }
        } else {
            ImGui::Text("Logging in progress. Saving to: %s.", OUTPUT_DIRECTORY.c_str());
            if (ImGui::Button("Stop logging")) {
                ServiceManager::stopLogging();
            }
        }
        ImGui::EndChild();
    }

    DbRow dataToDisplay;
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto row = playbackManager->getCurrentRow()) {
            dataToDisplay = *row;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        dataToDisplay = m_latestData;
    }

    // ESTRAZIONE DATI PACCHETTI PERSI
    std::string lostA = dataToDisplay.count("lost_packets_A") ? dataToDisplay.at("lost_packets_A") : "0";
    std::string lostB = dataToDisplay.count("lost_packets_B") ? dataToDisplay.at("lost_packets_B") : "0";
    std::string lostTot = dataToDisplay.count("lost_packets_TOT") ? dataToDisplay.at("lost_packets_TOT") : "0";

    // TABELLA PACCHETTI PERSI
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Lost packets");
    ImGui::PopFont();
    ImGui::Separator();
    
    if (ImGui::BeginTable("LostPacketsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Type"); 
        ImGui::TableSetupColumn("Count");
        ImGui::TableHeadersRow();

        // Riga Tipo A
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);         
        ImGui::TextUnformatted("0xCC (5 ms)");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_data); 
        ImGui::TextUnformatted(lostA.c_str());
        ImGui::PopFont();

        // Riga Tipo B
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);         
        ImGui::TextUnformatted("0xEE (200 ms)");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_data); 
        ImGui::TextUnformatted(lostB.c_str());
        ImGui::PopFont();

        // Riga Totali
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);         
        ImGui::TextUnformatted("Total");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_data); 
        ImGui::TextUnformatted(lostTot.c_str());
        ImGui::PopFont();

        ImGui::EndTable();
    }
    
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Current data");
    ImGui::PopFont();
    ImGui::Separator();

    // TABELLA PRINCIPALE DEI DATI
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type"); 
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        auto columnOrder = PacketParser::getColumnOrder();
        
        for (const auto& label : columnOrder) {
            // Salta i dati che abbiamo appena mostrato separatamente nella tabella sopra
            if (label == "lost_packets_A" || label == "lost_packets_B" || label == "lost_packets_tot") continue;
            
            if (dataToDisplay.count(label) == 0) continue; 
            
            const std::string& value = dataToDisplay.at(label);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);         
            ImGui::TextUnformatted(label.c_str());
            
            ImGui::TableSetColumnIndex(1);
            std::string displayValue = value;
            ImGui::PushFont(m_uiManager->font_data); 
            ImGui::TextUnformatted(displayValue.c_str());
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}