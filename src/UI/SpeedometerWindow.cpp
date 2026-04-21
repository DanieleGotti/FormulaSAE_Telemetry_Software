#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "UI/SpeedometerWindow.hpp"
#include "UI/UIManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SpeedometerWindow::SpeedometerWindow(UiManager* manager) : m_uiManager(manager) {}

void SpeedometerWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (dataRow.count("mean_velocity")) {
            m_meanVelocity = std::stof(dataRow.at("mean_velocity"));
        }
    } catch (const std::exception& e) {}
}

void SpeedometerWindow::draw() {
    ImGui::Begin("Speedometer");

    float currentSpeed = 0.0f;

    if (m_uiManager->getCurrentState() == AppState::CONNECTED_PLAYBACK) {
        auto playbackManager = ServiceManager::getPlaybackManager();
        if (auto rowOpt = playbackManager->getCurrentRow()) {
            try {
                if (rowOpt->count("mean_velocity")) currentSpeed = std::stof(rowOpt->at("mean_velocity"));
            } catch (...) {}
        }
    } else {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        currentSpeed = m_meanVelocity;
    }

    ImGui::Spacing();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 available_space = ImGui::GetContentRegionAvail();
    
    // --- RIDUZIONE DEL 10% E CENTRATURA ---
    float bottom_padding = 30.0f;
    // Moltiplicatore abbassato a 0.8f (10% in meno rispetto allo sterzo che era 0.9f)
    float image_size = std::min(available_space.x, available_space.y - bottom_padding) * 0.8f;
    float radius = image_size * 0.5f;

    // Coordinata centro (IDENTICA a SteerWindow)
    ImVec2 center(window_pos.x + available_space.x * 0.5f, window_pos.y + (available_space.y - bottom_padding) * 0.5f);

    float start_angle = 3.0f * M_PI / 4.0f;  // 135 gradi
    float end_angle   = 9.0f * M_PI / 4.0f;  // 405 gradi
    float angle_span  = end_angle - start_angle;
    
    float max_speed = 180.0f;
    float normalized_speed = std::clamp(currentSpeed / max_speed, 0.0f, 1.0f);
    float current_angle = start_angle + (normalized_speed * angle_span);

    // 1. TRACCIATO DI SFONDO (Più spesso e massiccio)
    ImU32 trackColor = m_uiManager->m_isDarkTheme ? IM_COL32(40, 40, 40, 255) : IM_COL32(200, 200, 200, 255);
    draw_list->PathClear();
    draw_list->PathArcTo(center, radius, start_angle, end_angle, 60);
    draw_list->PathStroke(trackColor, ImDrawFlags_None, 14.0f);

    // 2. TACCHE E NUMERI (Effetto Moto)
    ImU32 tickColor = m_uiManager->m_isDarkTheme ? IM_COL32(180, 180, 180, 255) : IM_COL32(80, 80, 80, 255);
    ImGui::PushFont(m_uiManager->font_body); 
    
    for (int i = 0; i <= 180; i += 10) {
        float a = start_angle + (i / 180.0f) * angle_span;
        ImVec2 dir(std::cos(a), std::sin(a));
        bool isMajor = (i % 30 == 0); // Tacca grande ogni 30 km/h
        
        float tickLen = isMajor ? 14.0f : 6.0f;
        float tickThick = isMajor ? 3.0f : 1.5f;
        
        ImVec2 p1(center.x + dir.x * (radius - tickLen), center.y + dir.y * (radius - tickLen));
        ImVec2 p2(center.x + dir.x * radius, center.y + dir.y * radius);
        draw_list->AddLine(p1, p2, tickColor, tickThick);
        
        if (isMajor) {
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%d", i);
            ImVec2 txt_size = ImGui::CalcTextSize(lbl);
            // Numeri spostati verso l'interno
            ImVec2 txt_pos(center.x + dir.x * (radius - 30.0f) - txt_size.x * 0.5f, 
                           center.y + dir.y * (radius - 30.0f) - txt_size.y * 0.5f);
            draw_list->AddText(txt_pos, tickColor, lbl);
        }
    }
    ImGui::PopFont();

    // 3. ARCO AZZURRO (Velocità attiva)
    if (normalized_speed > 0.001f) {
        ImU32 blueColor = IM_COL32(0, 200, 255, 255); 
        draw_list->PathClear();
        draw_list->PathArcTo(center, radius, start_angle, current_angle, 40);
        draw_list->PathStroke(blueColor, ImDrawFlags_None, 14.0f);
    }

    // 4. LANCETTA MODERNA (Poligono a Cuneo)
    ImU32 redColor = IM_COL32(230, 20, 20, 255);
    ImVec2 n_dir(std::cos(current_angle), std::sin(current_angle));
    ImVec2 n_perp(-std::sin(current_angle), std::cos(current_angle));
    
    ImVec2 pts[4];
    pts[0] = ImVec2(center.x + n_dir.x * (radius - 10.0f), center.y + n_dir.y * (radius - 10.0f)); // Punta
    pts[1] = ImVec2(center.x + n_perp.x * 4.0f, center.y + n_perp.y * 4.0f); // Base Destra
    pts[2] = ImVec2(center.x - n_dir.x * 12.0f, center.y - n_dir.y * 12.0f); // Coda (Dietro il centro)
    pts[3] = ImVec2(center.x - n_perp.x * 4.0f, center.y - n_perp.y * 4.0f); // Base Sinistra
    draw_list->AddConvexPolyFilled(pts, 4, redColor);

    // 5. HUB CENTRALE (Due cerchi concentrici metallici)
    ImU32 hubOuter = m_uiManager->m_isDarkTheme ? IM_COL32(30, 30, 30, 255) : IM_COL32(100, 100, 100, 255);
    ImU32 hubInner = m_uiManager->m_isDarkTheme ? IM_COL32(80, 80, 80, 255) : IM_COL32(50, 50, 50, 255);
    draw_list->AddCircleFilled(center, 12.0f, hubOuter);
    draw_list->AddCircleFilled(center, 6.0f, hubInner);

    // 7. TESTO ALLINEAMENTO ASSOLUTO (Identico allo SteerWindow)
    // 7. TESTO ALLINEAMENTO ASSOLUTO (Ancorato al fondo)
    char angle_text[16];
    ImGui::PushFont(m_uiManager->font_label);
    snprintf(angle_text, 16, "%.1f km/h", currentSpeed);
    ImVec2 angle_text_size = ImGui::CalcTextSize(angle_text);
    
    // Manteniamo la X centrata rispetto al mozzo centrale.
    // La Y è calcolata partendo dal fondo totale disponibile, sottraendo l'altezza del font e un piccolo margine (es. 10 pixel).
    ImVec2 text_pos(
        center.x - angle_text_size.x * 0.5f, 
        window_pos.y + available_space.y - angle_text_size.y - 20.0f 
    );
    
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), angle_text);
    ImGui::PopFont();

    ImGui::End();
}