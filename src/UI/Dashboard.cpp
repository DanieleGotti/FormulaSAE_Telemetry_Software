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
    ImGui::Begin("Gestione dati");

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_LIVE) {
        ImGui::BeginChild("ControlPanel", ImVec2(0, 100), true);
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Controllo registrazione");
        ImGui::PopFont();
        ImGui::Separator();
        
        bool isLogging = ServiceManager::isLogging();
        if (!isLogging) {
            if (ImGui::Button("Avvia registrazione")) {
                ServiceManager::startLogging(OUTPUT_DIRECTORY);
            }
        } else {
            ImGui::Text("Registrazione in corso. Salvataggio in: %s.", OUTPUT_DIRECTORY.c_str());
            if (ImGui::Button("Ferma registrazione")) {
                ServiceManager::stopLogging();
            }
        }
        ImGui::EndChild();
    }

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Dati correnti");
    ImGui::PopFont();
    ImGui::Separator();

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

    // STAMPA PACCHETTI PERSI SOPRA LA TABELLA
    std::string lostA = dataToDisplay.count("LOST_PACKETS_A") ? dataToDisplay.at("LOST_PACKETS_A") : "0";
    std::string lostB = dataToDisplay.count("LOST_PACKETS_B") ? dataToDisplay.at("LOST_PACKETS_B") : "0";
    std::string lostTot = dataToDisplay.count("LOST_PACKETS_TOT") ? dataToDisplay.at("LOST_PACKETS_TOT") : "0";

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Pacchetti Persi -> Tipo A (5ms): %s | Tipo B (200ms): %s | Totali: %s", lostA.c_str(), lostB.c_str(), lostTot.c_str());
    ImGui::PopFont();
    ImGui::Separator();

    // TABELLA
    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Tipo"); 
        ImGui::TableSetupColumn("Valore");
        ImGui::TableHeadersRow();

        auto columnOrder = PacketParser::getColumnOrder();
        
        for (const auto& label : columnOrder) {
            // Salta i dati che abbiamo appena mostrato separatamente
            if (label == "timestamp" || label == "LOST_PACKETS_A" || label == "LOST_PACKETS_B" || label == "LOST_PACKETS_TOT") continue;
            
            if (dataToDisplay.count(label) == 0) continue; 
            
            const std::string& value = dataToDisplay.at(label);

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
            ImGui::PushFont(m_uiManager->font_data); 
            ImGui::TextUnformatted(displayValue.c_str());
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}