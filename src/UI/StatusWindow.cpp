#include <imgui.h>
#include <string>
#include <sstream>
#include "UI/StatusWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

StatusWindow::StatusWindow(UiManager* manager) : m_uiManager(manager) {}

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

void StatusWindow::draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Pannello stati", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    bool r2dState = false;
    bool resetState = false;
    bool sdcState = false;
    bool tsOnState = false;
    std::string leftFsm = "NULL";
    std::string rightFsm = "NULL";

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        // Modalità lettura file
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto rowOpt = playbackManager->getCurrentRow()) {
            const DbRow& row = *rowOpt;
            try {
                if (row.count("R2D_BUTTON")) r2dState = (std::stoi(row.at("R2D_BUTTON")) == 1);
                if (row.count("RESET_BUTTON")) resetState = (std::stoi(row.at("RESET_BUTTON")) == 1);
                if (row.count("SDC_INPUT")) sdcState = (std::stoi(row.at("SDC_INPUT")) == 1);
                if (row.count("TS_ON_BUTTON")) tsOnState = (std::stoi(row.at("TS_ON_BUTTON")) == 1);
                if (row.count("LEFT_INVERTER_FSM")) leftFsm = row.at("LEFT_INVERTER_FSM");
                if (row.count("RIGHT_INVERTER_FSM")) rightFsm = row.at("RIGHT_INVERTER_FSM");
            } catch (const std::exception& e) {
                // Errore di parsing, i valori di default verranno usati.
            }
        }
    } else {
        // Modalità live
        std::lock_guard<std::mutex> lock(m_dataMutex);
        r2dState = m_r2dButtonState;
        resetState = m_resetButtonState;
        sdcState = m_sdcInputState;
        tsOnState = m_tsOnButtonState;
        leftFsm = m_leftInverterFsm;
        rightFsm = m_rightInverterFsm;
    }

    // Pannello indicatori LED
    ImGui::BeginChild("LeftPane", ImVec2(m_leftPaneWidth, 0), false, ImGuiWindowFlags_NoBackground);
    ImGui::SetCursorPos(ImVec2(10, 10));
    ImGui::BeginGroup();
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Indicatori di stato");
    ImGui::PopFont();
    ImGui::Separator();
    
    drawLed("TS On", tsOnState);
    ImGui::SameLine(0, 20.f);
    drawLed("SDC", sdcState);
    ImGui::SameLine(0, 20.f);
    drawLed("R2D", r2dState);
    ImGui::SameLine(0, 20.f);
    drawLed("Reset", resetState);

    ImGui::EndGroup();
    ImGui::EndChild();

    ImGui::SameLine();

    // Separatore 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Button("##v_splitter", ImVec2(8.0f, -1));
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        m_leftPaneWidth += ImGui::GetIO().MouseDelta.x;
        if (m_leftPaneWidth < 150.0f) m_leftPaneWidth = 150.0f;
        float max_width = ImGui::GetWindowWidth() - 250.0f; 
        if (m_leftPaneWidth > max_width) m_leftPaneWidth = max_width;
    }

    ImGui::SameLine();

    // Pannello Inverter
    ImGui::BeginChild("RightPane", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);
    ImGui::SetCursorPos(ImVec2(10, 10));
    ImGui::BeginGroup();
    ImGui::PushFont(m_uiManager->font_label);
    ImGui::Text("Stati inverter");
    ImGui::PopFont();
    ImGui::Separator();
    
    float available_width = ImGui::GetContentRegionAvail().x;
    float panel_width = (available_width - ImGui::CalcTextSize("Left: Right: ").x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    if (panel_width < 50.0f) panel_width = 50.0f;
    float panel_height = ImGui::GetFrameHeightWithSpacing();

    ImGui::PushFont(m_uiManager->font_body); ImGui::Text("Left:"); ImGui::PopFont();
    ImGui::SameLine();
    ImGui::BeginChild("LeftInverterPanel", ImVec2(panel_width, panel_height), true);
    ImGui::PushFont(m_uiManager->font_body);
    ImVec2 text_size = ImGui::CalcTextSize(leftFsm.c_str());
    ImVec2 panel_size = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panel_size.x - text_size.x) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (panel_size.y - text_size.y) * 0.5f);
    ImGui::Text("%s", leftFsm.c_str());
    ImGui::PopFont();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::PushFont(m_uiManager->font_body); ImGui::Text("Right:"); ImGui::PopFont();
    ImGui::SameLine();
    ImGui::BeginChild("RightInverterPanel", ImVec2(panel_width, panel_height), true);
    ImGui::PushFont(m_uiManager->font_body);
    text_size = ImGui::CalcTextSize(rightFsm.c_str());
    panel_size = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panel_size.x - text_size.x) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (panel_size.y - text_size.y) * 0.5f);
    ImGui::Text("%s", rightFsm.c_str());
    ImGui::PopFont();
    ImGui::EndChild();

    ImGui::EndGroup();
    ImGui::EndChild(); 
    
    ImGui::End();
}