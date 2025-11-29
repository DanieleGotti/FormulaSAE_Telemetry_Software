#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm> 
#include "UI/ModulesBatteryWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

ModulesBatteryWindow::ModulesBatteryWindow(UiManager* manager) : m_uiManager(manager) {}

void ModulesBatteryWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_latestData = dataRow;
}

// Logica colori temperatura
ImVec4 ModulesBatteryWindow::getTempColor(float value) {
    if (value < 50.0f) return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);       // verde
    else if (value < 65.0f) return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);  // giallo
    else return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);                     // rosso
}

// Logica colori tensione
ImVec4 ModulesBatteryWindow::getVoltageColor(float value) {
    if (value >= 3.6f && value <= 4.2f) return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); 
    else if (value > 3.0f && value < 4.5f) return ImVec4(1.0f, 0.8f, 0.0f, 1.0f); 
    else return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); 
}

void ModulesBatteryWindow::drawErrorLed(bool isError) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float radius = ImGui::GetTextLineHeight() * 0.35f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 center(p.x + radius, p.y + ImGui::GetTextLineHeight() / 2.0f);
    
    ImGui::Dummy(ImVec2(radius * 2 + 5, ImGui::GetTextLineHeight()));
    ImU32 color = isError ? IM_COL32(255, 0, 0, 255) : IM_COL32(40, 40, 40, 255);
    draw_list->AddCircleFilled(center, radius, color);
    draw_list->AddCircle(center, radius, IM_COL32(150, 150, 150, 255));
}

void ModulesBatteryWindow::drawModuleCard(int index, const DbRow& dataMap) {
    std::string idxStr = std::to_string(index);
    std::string keyTmp1 = "TMP1M" + idxStr;
    std::string keyTmp2 = "TMP2M" + idxStr;
    std::string keyTens = "TENSM" + idxStr;
    std::string keyErr  = "ERRORM" + idxStr;

    ImGui::PushID(index);

    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Modulo %d", index); 
    ImGui::PopFont();
    ImGui::Separator();

    // Helper lambda ottimizzata per layout compatto
    auto drawValue = [&](const char* label, const std::string& key, bool isTemp) {
        ImGui::Text("%s", label);
        ImGui::SameLine();
        
        auto it = dataMap.find(key);
        if (it != dataMap.end()) {
            try {
                float val = std::stof(it->second);
                ImVec4 color = isTemp ? getTempColor(val) : getVoltageColor(val);
                
                ImGui::PushFont(m_uiManager->font_label);
                ImGui::TextColored(color, "%.1f", val); 
                ImGui::SameLine(0.0f, 1.0f); 
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", isTemp ? " °C" : " V");
                ImGui::PopFont();
            } catch (...) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "NaN");
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "--");
        }
    };

    ImGui::BeginGroup();
    
    drawValue("T1:", keyTmp1, true);
    drawValue("T2:", keyTmp2, true);
    drawValue("V  :", keyTens, false); 

    // LED Errore
    ImGui::Text("Errore:");
    ImGui::SameLine();
    bool isError = false;
    auto itErr = dataMap.find(keyErr);
    if (itErr != dataMap.end()) {
        try { isError = (std::stof(itErr->second) != 0.0f); } catch(...) {}
    }
    drawErrorLed(isError);

    ImGui::EndGroup();
    ImGui::PopID();
}

void ModulesBatteryWindow::draw() {
    ImGui::Begin("Moduli batteria");

    DbRow dataToDisplay;
    bool hasData = false;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto row = playbackManager->getCurrentRow()) {
            dataToDisplay = *row;
            hasData = true;
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_latestData.empty()) {
            dataToDisplay = m_latestData;
            hasData = true;
        }
    }

    if (hasData) {
        // Logica layout dinamico
        float availWidth = ImGui::GetContentRegionAvail().x;
        float minModuleWidth = 110.0f; 
        int columns = static_cast<int>(availWidth / minModuleWidth);
        
        if (columns < 1) columns = 1;
        if (columns > 14) columns = 14;
        if (ImGui::BeginTable("ModulesGrid", columns, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingStretchProp)) {
            
            for (int i = 1; i <= 14; ++i) {
                ImGui::TableNextColumn();
                drawModuleCard(i, dataToDisplay);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("In attesa di dati.");
    }

    ImGui::End();
}