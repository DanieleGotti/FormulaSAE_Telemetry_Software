#include <imgui.h>
#include <string>
#include <sstream>

#include "UI/StatusWindow.hpp"
#include "UI/UIManager.hpp"

// Funzione helper per estrarre l'ultimo stato (rimane invariata)
static std::string getLastState(const std::string& states) {
    size_t last_separator = states.rfind(" / ");
    if (last_separator != std::string::npos) {
        return states.substr(last_separator + 3);
    }
    return states;
}

StatusWindow::StatusWindow(UiManager* manager)
    : m_uiManager(manager) {}

void StatusWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (dataRow.count("R2D_BUTTON")) m_r2dButtonState = (std::stoi(dataRow.at("R2D_BUTTON")) == 1);
        if (dataRow.count("RESET_BUTTON")) m_resetButtonState = (std::stoi(dataRow.at("RESET_BUTTON")) == 1);
        if (dataRow.count("SDC_INPUT")) m_sdcInputState = (std::stoi(dataRow.at("SDC_INPUT")) == 1);
        if (dataRow.count("TS_ON_BUTTON")) m_tsOnButtonState = (std::stoi(dataRow.at("TS_ON_BUTTON")) == 1);
        if (dataRow.count("LEFT_INVERTER_FSM")) m_leftInverterFsm = dataRow.at("LEFT_INVERTER_FSM");
        if (dataRow.count("RIGHT_INVERTER_FSM")) m_rightInverterFsm = dataRow.at("RIGHT_INVERTER_FSM");
    } catch (const std::exception& e) {}
}

// Funzione per disegnare i LED
void StatusWindow::drawLed(const char* label, bool state) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float radius = ImGui::GetTextLineHeight() * 0.4f;
    float diameter = radius * 2.0f;
    float vertical_padding = ImGui::GetTextLineHeight() * 0.1f;
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 center(p.x + radius, p.y + radius + vertical_padding);

    ImGui::Dummy(ImVec2(diameter, diameter + vertical_padding * 2));

    ImU32 color = state ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
    draw_list->AddCircleFilled(center, radius, color);

    ImGui::SameLine();
    ImGui::PushFont(m_uiManager->font_body);
    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vertical_padding);
    ImGui::Text("%s", label);
    ImGui::PopFont();
}

void StatusWindow::draw() {
    ImGui::Begin("Pannello stati");

    // Crea una tabella a 2 colonne con linea verticale 
    if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        
        ImGui::TableNextColumn();
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Indicatori di stato");
        ImGui::PopFont();
        ImGui::Separator();

        ImGui::TableNextColumn();
        ImGui::PushFont(m_uiManager->font_label);
        ImGui::Text("Stati inverter");
        ImGui::PopFont();
        ImGui::Separator();
    
        ImGui::TableNextRow();

        // LED
        ImGui::TableNextColumn();
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            drawLed("TS On", m_tsOnButtonState);
            ImGui::SameLine(0, 20.f);
            drawLed("SDC", m_sdcInputState);
            ImGui::SameLine(0, 20.f);
            drawLed("R2D", m_r2dButtonState);
            ImGui::SameLine(0, 20.f);
            drawLed("Reset", m_resetButtonState);
            
        }

        // Pannello inverter
        ImGui::TableNextColumn();
        {
            std::string last_left_state, last_right_state;
            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                last_left_state = getLastState(m_leftInverterFsm);
                last_right_state = getLastState(m_rightInverterFsm);
            }

            float panel_width = 300.0f; 
            float panel_height = 36.0f;

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);

            // Inverter Sinistro
            ImGui::PushFont(m_uiManager->font_body);
            ImGui::Text("Left:");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::BeginChild("LeftInverterPanel", ImVec2(panel_width, panel_height), true);
            ImGui::PushFont(m_uiManager->font_data);
            ImVec2 text_size = ImGui::CalcTextSize(last_left_state.c_str()); 
            ImVec2 panel_size = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panel_size.x - text_size.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (panel_size.y - text_size.y) * 0.5f);
            ImGui::Text("%s", last_left_state.c_str());
            ImGui::PopFont();
            ImGui::EndChild();

            ImGui::SameLine(0, 30.f); 

            // Inverter Destro
            ImGui::PushFont(m_uiManager->font_body);
            ImGui::Text("Right:");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::BeginChild("RightInverterPanel", ImVec2(panel_width, panel_height), true); 
            ImGui::PushFont(m_uiManager->font_data);
            text_size = ImGui::CalcTextSize(last_right_state.c_str());
            panel_size = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panel_size.x - text_size.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (panel_size.y - text_size.y) * 0.5f);
            ImGui::Text("%s", last_right_state.c_str());
            ImGui::PopFont();
            ImGui::EndChild();
        }

        ImGui::EndTable();
    }
    
    ImGui::End();
}