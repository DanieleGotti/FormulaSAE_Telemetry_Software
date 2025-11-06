#include <imgui.h>
#include <string>
#include <cmath>
#include <iostream>
#include <array>
#include <algorithm>
#include <GLFW/glfw3.h>
#include "UI/SteerWindow.hpp"
#include "UI/UIManager.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image/stb_image.h"

#ifndef PI
#define PI 3.1415926535f
#endif

// Funzione per ruotare l'immagine dello sterzo
static std::array<ImVec2, 4> GetRotatedQuadPoints(const ImVec2& center, float size, float angle_degrees) {
    float angle_rad = angle_degrees * PI / 180.0f;
    float s = sinf(angle_rad);
    float c = cosf(angle_rad);
    float half_size = size * 0.5f;
    std::array<ImVec2, 4> points = {
        {{-half_size, -half_size}, {half_size, -half_size}, {half_size, half_size}, {-half_size, half_size}}
    };
    for (auto& p : points) {
        float new_x = p.x * c - p.y * s;
        float new_y = p.x * s + p.y * c;
        p.x = center.x + new_x;
        p.y = center.y + new_y;
    }
    return points;
}


SteerWindow::SteerWindow(UiManager* manager) : m_uiManager(manager) {
    bool ret = loadTextureFromFile("../external/assets/steering_wheel.png", &m_wheelTextureID, &m_wheelWidth, &m_wheelHeight);
    if (!ret) {
        std::cerr << "ERRORE [SteerWindow]: Impossibile caricare l'immagine 'steering_wheel.png'" << std::endl;
    }
}

SteerWindow::~SteerWindow() {
    if (m_wheelTextureID) {
        glDeleteTextures(1, &m_wheelTextureID);
    }
}

void SteerWindow::onAggregatedDataReceived(const DbRow& dataRow) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    try {
        if (dataRow.count("STEER")) { 
            m_steeringAngle = std::stof(dataRow.at("STEER"));
        }
    } catch (const std::exception& e) {}
}

void SteerWindow::draw() {
    ImGui::Begin("Sterzo");

    float current_angle;
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        current_angle = m_steeringAngle;
    }

    // Mostra il valore intero
    ImGui::PushFont(m_uiManager->font_data);
    char angle_text[16];
    snprintf(angle_text, 16, "%.1f°", current_angle);
    ImVec2 text_size = ImGui::CalcTextSize(angle_text);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_size.x) * 0.5f);
    ImGui::TextUnformatted(angle_text);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    // Disegna il volante
    if (m_wheelTextureID) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 available_space = ImGui::GetContentRegionAvail();
        float image_size = std::min(available_space.x, available_space.y) * 0.9f;
        ImVec2 image_center(window_pos.x + available_space.x * 0.5f, window_pos.y + available_space.y * 0.5f);

        // Disegna l'immagine del volante ruotata
        auto vertices = GetRotatedQuadPoints(image_center, image_size, current_angle);
        ImU32 tint_color = m_uiManager->m_isDarkTheme
            ? IM_COL32(200, 200, 200, 255)   // Tema scuro -> volante grigio chiaro
            : IM_COL32(80, 80, 80, 255);     // Tema chiaro -> volante grigio scuro

        draw_list->AddImageQuad(
            (void*)(intptr_t)m_wheelTextureID,
            vertices[0], vertices[1], vertices[2], vertices[3],
            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
            tint_color
        );

        // Linea fissa in alto che indica il centro
        float fixed_line_length = 15.0f;
        ImVec2 fixed_line_start(image_center.x, window_pos.y);
        ImVec2 fixed_line_end(image_center.x, window_pos.y + fixed_line_length);
        ImU32 fixed_line_color = m_uiManager->m_isDarkTheme
            ? IM_COL32(255, 255, 255, 255) // Tema scuro -> linea bianca
            : IM_COL32(0, 0, 0, 255);       // Tema chiaro -> linea nera
        draw_list->AddLine(fixed_line_start, fixed_line_end, fixed_line_color, 2.0f);

        // Linea rossa che ruota insieme al volante
        float angle_rad = current_angle * PI / 180.0f;
        float start_radius = image_size * 0.35f; 
        float end_radius = image_size * 0.45f;   

        ImVec2 rotating_line_start(
            image_center.x + sinf(angle_rad) * start_radius,
            image_center.y - cosf(angle_rad) * start_radius
        );

        ImVec2 rotating_line_end(
            image_center.x + sinf(angle_rad) * end_radius,
            image_center.y - cosf(angle_rad) * end_radius
        );
        draw_list->AddLine(rotating_line_start, rotating_line_end, IM_COL32(255, 0, 0, 255), 3.0f);
    }

    ImGui::End();
}

bool SteerWindow::loadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    int image_width = 0, image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL) return false;

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;
    return true;
}