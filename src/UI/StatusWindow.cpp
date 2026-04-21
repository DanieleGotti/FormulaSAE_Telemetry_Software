#include <imgui.h>
#include <string>
#include <algorithm>
#include "UI/StatusWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

// Funzioni helper per ricavare il numero dalla stringa
static std::string formatInverterFsm(const std::string& stateStr) {
    const char* states[] = {"OFF", "SYSTEM_READY", "DC_CAPACITOR_CHARGE", "DC_OK", 
                            "ENABLE_INVERTER", "ENABLED", "ON", "OK", "ERROR"};
    for (int i = 0; i <= 8; i++) {
        if (stateStr == states[i]) return "(" + std::to_string(i) + ") " + stateStr;
    }
    return stateStr;
}

static std::string formatTractiveFsm(const std::string& stateStr) {
    const char* states[] = {"ENTRY_STATE", "START_LIGHT_TEST", "WAIT_LIGHT_TEST", "POWER_OFF", 
                            "TS_WAIT_ACTIVATION", "TS_WAIT_PRECHARGE", "TS_ACTIVE", "R2D_WAIT", 
                            "R2D_ACTIVE", "ERROR_STATE"};
    for (int i = 0; i <= 9; i++) {
        if (stateStr == states[i]) return "(" + std::to_string(i) + ") " + stateStr;
    }
    return stateStr;
}

static std::string formatEcuMode(const std::string& stateStr) {
    if (stateStr == "ENDURANCE") return "(0x01) ENDURANCE";
    if (stateStr == "ACCELERATION") return "(0x02) ACCELERATION";
    if (stateStr == "TEST") return "(0x04) TEST";
    return stateStr;
}

StatusWindow::StatusWindow(UiManager* manager) : m_uiManager(manager) {}

void StatusWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (dataRow.count("sdc")) m_sdcInputState = (std::stoi(dataRow.at("sdc")) == 1);
        if (dataRow.count("ready_to_drive_button")) m_r2dButtonState = (std::stoi(dataRow.at("ready_to_drive_button")) == 1);
        if (dataRow.count("ecu_reset_button")) m_resetButtonState = (std::stoi(dataRow.at("ecu_reset_button")) == 1);
        if (dataRow.count("tractive_system_on_button")) m_tsOnButtonState = (std::stoi(dataRow.at("tractive_system_on_button")) == 1);
        if (dataRow.count("left_inverter_fsm")) m_leftInverterFsm = dataRow.at("left_inverter_fsm");
        if (dataRow.count("right_inverter_fsm")) m_rightInverterFsm = dataRow.at("right_inverter_fsm");
        if (dataRow.count("tractive_system_fsm")) m_tractiveSystemFsm = dataRow.at("tractive_system_fsm");
        if (dataRow.count("ECU_Mode")) m_ecuMode = dataRow.at("ECU_Mode");
        if (dataRow.count("state_of_charge")) m_stateOfCharge = std::stoi(dataRow.at("state_of_charge"));
    } catch (const std::exception& e) {}
}

void StatusWindow::drawLed(const char* label, bool state) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float radius = ImGui::GetTextLineHeight() * 0.4f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 center(p.x + radius, p.y + ImGui::GetTextLineHeight() / 2.0f);
    ImGui::Dummy(ImVec2(radius * 2, ImGui::GetTextLineHeight()));
    ImU32 color = state ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
    draw_list->AddCircleFilled(center, radius, color);
    ImGui::SameLine();
    ImGui::Text("%s", label);
}

void StatusWindow::drawBattery(int soc) {
    // Stessa altezza calcolata per i campi FSM: box_height = ImGui::GetTextLineHeightWithSpacing() * 1.25f;
    float h = ImGui::GetTextLineHeightWithSpacing() * 1.5f; 
    float w = 84.0f;  
    float polo_w = 4.0f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Colori
    ImU32 outlineColor = m_uiManager->m_isDarkTheme ? IM_COL32(220, 220, 220, 255) : IM_COL32(40, 40, 40, 255);
    
    // Corpo e polo
    draw_list->AddRect(p, ImVec2(p.x + w, p.y + h), outlineColor, 2.0f, 0, 2.0f);
    draw_list->AddRectFilled(ImVec2(p.x + w, p.y + h * 0.3f), ImVec2(p.x + w + polo_w, p.y + h * 0.7f), outlineColor, 1.0f);

    // Riempimento dinamico
    ImU32 fillColor;
    if (soc > 50) fillColor = IM_COL32(0, 200, 0, 255);        
    else if (soc > 20) fillColor = IM_COL32(255, 180, 0, 255); 
    else fillColor = IM_COL32(255, 0, 0, 255);                 

    float fillWidth = (w - 4.0f) * (std::clamp(soc, 0, 100) / 100.0f);
    if (fillWidth > 1.0f) {
        draw_list->AddRectFilled(ImVec2(p.x + 2.0f, p.y + 2.0f), ImVec2(p.x + 2.0f + fillWidth, p.y + h - 2.0f), fillColor, 1.0f);
    }

    // Dummy per occupare l'ingombro reale nel layout di ImGui
    ImGui::Dummy(ImVec2(w + polo_w, h));
}

void StatusWindow::draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    
    // RIMOSSI I FLAG DI NO-SCROLL: ora se stringi verso l'alto apparirà la scrollbar
    ImGui::Begin("Pannello stati", nullptr); 
    ImGui::PopStyleVar();

    bool r2dState = false; 
    bool resetState = false; 
    bool sdcState = false; 
    bool tsOnState = false;
    std::string leftFsm = "NULL"; 
    std::string rightFsm = "NULL";
    std::string tsFsm = "NULL";
    std::string ecuMode = "NULL";
    int currentSoc = 0;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto rowOpt = playbackManager->getCurrentRow()) {
            const DbRow& row = *rowOpt;
            try {
                if (row.count("sdc")) sdcState = (std::stoi(row.at("sdc")) == 1);
                if (row.count("ready_to_drive_button")) r2dState = (std::stoi(row.at("ready_to_drive_button")) == 1);
                if (row.count("ecu_reset_button")) resetState = (std::stoi(row.at("ecu_reset_button")) == 1);
                if (row.count("tractive_system_on_button")) tsOnState = (std::stoi(row.at("tractive_system_on_button")) == 1);
                if (row.count("left_inverter_fsm")) leftFsm = row.at("left_inverter_fsm");
                if (row.count("right_inverter_fsm")) rightFsm = row.at("right_inverter_fsm");
                if (row.count("tractive_system_fsm")) tsFsm = row.at("tractive_system_fsm");
                if (row.count("state_of_charge")) currentSoc = std::stoi(row.at("state_of_charge"));
                if (row.count("ECU_Mode")) ecuMode = row.at("ECU_Mode");
            } catch (const std::exception& e) {}
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        r2dState = m_r2dButtonState; 
        resetState = m_resetButtonState;
        sdcState = m_sdcInputState; 
        tsOnState = m_tsOnButtonState;
        leftFsm = m_leftInverterFsm; 
        rightFsm = m_rightInverterFsm; 
        tsFsm = m_tractiveSystemFsm;
        ecuMode = m_ecuMode;
        currentSoc = m_stateOfCharge;
    }

    std::string formattedLeft = formatInverterFsm(leftFsm);
    std::string formattedRight = formatInverterFsm(rightFsm);
    std::string formattedTs = formatTractiveFsm(tsFsm);
    std::string formattedEcuMode = formatEcuMode(ecuMode);

    if (ImGui::BeginTable("StatusTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
        
        ImGui::TableSetupColumn("LEDs", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("FSMs", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Battery", ImGuiTableColumnFlags_WidthFixed, 160.0f);

        // --- RIGA TITOLI ---
        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        ImGui::PushFont(m_uiManager->font_label); ImGui::Text("Digital inputs"); ImGui::PopFont();
        ImGui::Separator();

        ImGui::TableSetColumnIndex(1);
        ImGui::PushFont(m_uiManager->font_label); ImGui::Text("State machines"); ImGui::PopFont();
        ImGui::Separator();

        ImGui::TableSetColumnIndex(2);
        ImGui::PushFont(m_uiManager->font_label); ImGui::Text("State of charge"); ImGui::PopFont();
        ImGui::Separator();

        // --- RIGA CONTENUTI ---
        ImGui::TableNextRow();

        // CELLA 1: LED 
        ImGui::TableSetColumnIndex(0);
        ImGui::Spacing();
        ImGui::BeginGroup();
        drawLed("SDC", sdcState); ImGui::SameLine(100.0f); drawLed("R2D button", r2dState);
        drawLed("Reset button", resetState); ImGui::SameLine(100.0f); drawLed("TS ON button", tsOnState);
        ImGui::EndGroup();

        // CELLA 2: FSM 
        ImGui::TableSetColumnIndex(1);
        ImGui::Spacing();
        
        float avail_w = ImGui::GetContentRegionAvail().x;
        float item_spacing = ImGui::GetStyle().ItemSpacing.x;
        float box_width = (avail_w - (item_spacing * 3)) / 4.0f;
        if (box_width < 40.0f) box_width = 40.0f; 
        
        // <-- ALTEZZA RIDOTTA DEI CAMPI NERI (da 1.6f a 1.25f per renderli più snelli)
        float box_height = ImGui::GetTextLineHeightWithSpacing() * 1.5f; 

        auto drawFsmBox = [&](const char* title, const char* id, const std::string& text) {
            ImGui::BeginGroup();
            ImGui::PushFont(m_uiManager->font_body); 
            ImGui::Text("%s", title); 
            
            ImGui::BeginChild(id, ImVec2(box_width, box_height), true);
            ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
            ImGui::SetCursorPos(ImVec2((box_width - text_size.x) * 0.5f, (box_height - text_size.y) * 0.5f));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", text.c_str());
            ImGui::EndChild();
            
            ImGui::PopFont();
            ImGui::EndGroup();
        };

        drawFsmBox("Left inverter FSM:", "LeftInverterPanel", formattedLeft); ImGui::SameLine();
        drawFsmBox("Right inverter FSM:", "RightInverterPanel", formattedRight); ImGui::SameLine();
        drawFsmBox("Tractive system FSM:", "TsPanel", formattedTs); ImGui::SameLine();
        drawFsmBox("ECU mode:", "EcuModePanel", formattedEcuMode); 

        // CELLA 3: BATTERIA 
        ImGui::TableSetColumnIndex(2);
        ImGui::Spacing(); // Fondamentale: allinea la partenza al livello delle altre celle
        
        ImGui::BeginGroup();
        
        // --- 1. TESTO PERCENTUALE (Allineato in Y con i titoli FSM) ---
        char soc_text[16];
        snprintf(soc_text, sizeof(soc_text), "%d%%", currentSoc);
        
        // Centratura X del testo
        float cell_w = ImGui::GetContentRegionAvail().x;
        float text_w = ImGui::CalcTextSize(soc_text).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cell_w - text_w) * 0.5f);
        
        ImGui::PushFont(m_uiManager->font_label); // Usa lo stesso font dei titoli Inverter/TS
        ImGui::Text("%s", soc_text);
        ImGui::PopFont();
        
        // --- 2. ICONA BATTERIA (Allineata in Y con i box FSM neri) ---
        float batt_total_width = 84.0f + 4.0f; // Larghezza base (w) + polo_w
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cell_w - batt_total_width) * 0.5f);
        
        drawBattery(currentSoc);
        
        ImGui::EndGroup();

        ImGui::EndTable();
    }

    ImGui::End();
}