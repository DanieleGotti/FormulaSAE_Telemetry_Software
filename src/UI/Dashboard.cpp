#include <imgui.h>
#include <vector>
#include <string>
#include "UI/UIManager.hpp"
#include "UI/Dashboard.hpp"
#include "Telemetry/Services/ServiceManager.hpp"

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

    if (ImGui::BeginTable("DataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Tipo"); 
        ImGui::TableSetupColumn("Valore");

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Tipo"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("Valore");
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

        for (const auto& [label, value] : dataToDisplay) {
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
            ImGui::PushFont(m_uiManager->font_data); 
            ImGui::TextUnformatted(displayValue.c_str());
            ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}