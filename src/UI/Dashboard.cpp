#include <imgui.h>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <string>
#include <utils/fsUtils.hpp>
#include "UI/UIManager.hpp" 
#include "UI/Dashboard.hpp" 
#include "Telemetry/Services/ServiceManager.hpp" 
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

Dashboard::Dashboard(UiManager* manager) : m_uiManager(manager) {}

void Dashboard::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex); 
    m_latestData = dataRow; 
}

void Dashboard::draw() { 
    ImGui::Begin("Data management");

    // Safe, cross-platform path mapping to user documents/config folders
    std::filesystem::path outputDir = fsutils::getExecutableDirectory() / "output_data";

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_LIVE) {
        ImGui::BeginChild("Control panel", ImVec2(0, 100), true);
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Logging control");
        ImGui::PopFont();
        ImGui::Separator();
        
        bool isLogging = ServiceManager::isLogging();
        if (!isLogging) {
            if (ImGui::Button("Start logging")) {
                std::filesystem::create_directories(outputDir);
                ServiceManager::startLogging(outputDir.string());
            }
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Logging in progress... Saving to root output_data");
            if (ImGui::Button("Stop logging")) {
                ServiceManager::stopLogging();
            }
        }
        ImGui::EndChild();
    }
    
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        ImGui::BeginChild("Export tools", ImVec2(0, 100), true);
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Data Export");
        ImGui::PopFont();
        ImGui::Separator();
        
        auto playbackManager = ServiceManager::getPlaybackManager();

        // Disable button if not a BIN file OR if already exporting
        const bool canExport = playbackManager->canGenerateCsv();
        const bool isExporting = playbackManager->isExportingCsv();
        const bool disabled = !canExport || isExporting;

        if (disabled) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button(isExporting ? "Exporting... Please wait" : "Generate High-Res CSV (.csv)")) {
            std::filesystem::create_directories(outputDir);
            
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
            
            std::string outPath = (outputDir / ("exported_telemetry_" + ss.str() + ".csv")).string();
            
            // Runs safely on a detached background thread!
            playbackManager->generateCsvFromBin(outPath);
        }
        
        if (disabled) {
            ImGui::EndDisabled();
        }

        // Hint sits on its own line underneath the button
        if (!canExport) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Available only for .bin inputs)");
        }
        ImGui::EndChild();
    }

    DbRow dataToDisplay;
    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto row = playbackManager->getCurrentRow()) { // JIT Row creation
            dataToDisplay = *row;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        dataToDisplay = m_latestData;
    }

    // Extraction logic stays untouched
    std::string lostA = dataToDisplay.count("lost_packets_A") ? dataToDisplay.at("lost_packets_A") : "0";
    std::string lostB = dataToDisplay.count("lost_packets_B") ? dataToDisplay.at("lost_packets_B") : "0";
    std::string lostTot = dataToDisplay.count("lost_packets_tot") ? dataToDisplay.at("lost_packets_tot") : "0";

    // Lost packets table
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Lost packets");
    ImGui::PopFont();
    ImGui::Separator();
    
    if (ImGui::BeginTable("Lost packets table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Type"); 
        ImGui::TableSetupColumn("Count");
        ImGui::TableHeadersRow();

        // Type A lost packets
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);         
        ImGui::TextUnformatted("0xCC (5 ms)");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_data); 
        ImGui::TextUnformatted(lostA.c_str());
        ImGui::PopFont();

        // Type B lost packets
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);         
        ImGui::TextUnformatted("0xEE (200 ms)");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_data); 
        ImGui::TextUnformatted(lostB.c_str());
        ImGui::PopFont();

        // Total row
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

    // Data table
    if (ImGui::BeginTable("Data table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type"); 
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        // Rebuilt once: the schema order never changes while the app is running
        static const std::vector<std::string> columnOrder = PacketParser::getColumnOrder();

        auto drawRow = [&](const std::string& label, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::PushFont(m_uiManager->font_data);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopFont();
        };

        auto isHidden = [](const std::string& label) {
            // Lost packet counts have their own table above, and the per-module
            // online flags are drawn by the battery window
            return label == "lost_packets_A" || label == "lost_packets_B" || label == "lost_packets_tot"
                || label.rfind("OnlineModule", 0) == 0;
        };

        for (const auto& label : columnOrder) {
            if (isHidden(label)) continue;
            const auto it = dataToDisplay.find(label);
            if (it == dataToDisplay.end()) continue;
            drawRow(label, it->second);
        }

        // Channels this build does not know about (recording from another
        // firmware version) are listed after the known ones instead of vanishing
        for (const auto& entry : dataToDisplay) {
            if (isHidden(entry.first)) continue;
            if (std::find(columnOrder.begin(), columnOrder.end(), entry.first) != columnOrder.end()) continue;
            drawRow(entry.first, entry.second);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}