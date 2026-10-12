/**
 * @file CreditsWindow.cpp
 * @brief Implementation of the credits window (see CreditsWindow.hpp).
 */
#include <imgui.h>
#include <iostream>
#include "UI/UIManager.hpp"
#include "UI/CreditsWindow.hpp"
#include "utils/IAssetManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "../external/stb_image/stb_image.h" 

CreditsWindow::CreditsWindow(UiManager* manager) : m_uiManager(manager) {
    if (!loadTextureFromMemory("logo_erb.png", &m_textureID, &m_imgWidth, &m_imgHeight)) {
        std::cerr << "ERROR [CreditsWindow]: Failed to load image 'logo_erb.png'." << std::endl;
    }
}

CreditsWindow::~CreditsWindow() {
    if (m_textureID) glDeleteTextures(1, &m_textureID);
}

void CreditsWindow::draw() {
    // Visible only on the Home screen
    if (m_uiManager->getCurrentState() != AppState::CONFIGURING) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Credits", nullptr, ImGuiWindowFlags_NoCollapse);

    float window_width = ImGui::GetWindowSize().x;
    float window_height = ImGui::GetWindowSize().y;
    const char* authorsText = "Authors:      Daniele Gotti      Samuele Stasi"; 

    // Logo height keeping the aspect ratio at a fixed width
    float target_height = 0.0f;
    if (m_textureID && m_imgWidth > 0) {
        float aspect_ratio = static_cast<float>(m_imgHeight) / static_cast<float>(m_imgWidth);
        target_height = TARGET_IMAGE_WIDTH * aspect_ratio;
    }

    ImGui::PushFont(m_uiManager->font_body);
    float text_height = ImGui::CalcTextSize(authorsText).y;
    ImGui::PopFont();

    // Vertical centering of the logo + text block
    float spacing = ImGui::GetStyle().ItemSpacing.y; 
    float total_content_height = target_height + spacing + text_height;

    float start_y = (window_height - total_content_height) * 0.5f;
    if (start_y > 0) {
        ImGui::SetCursorPosY(start_y); 
    }

    if (m_textureID && m_imgWidth > 0) {
        ImGui::SetCursorPosX((window_width - TARGET_IMAGE_WIDTH) * 0.5f);
        
        // White logo on the dark theme, almost black on the light theme
        ImVec4 tint_col = m_uiManager->m_isDarkTheme ? 
                          ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : 
                          ImVec4(0.15f, 0.15f, 0.15f, 1.0f); 

        ImVec4 border_col = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        ImGui::Image((ImTextureID)(intptr_t)m_textureID, 
                     ImVec2(TARGET_IMAGE_WIDTH, target_height), 
                     ImVec2(0.0f, 0.0f), 
                     ImVec2(1.0f, 1.0f), 
                     tint_col, 
                     border_col);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing); 

    ImGui::Dummy(ImVec2(0.0f, 20.0f)); 

    // Authors centered horizontally
    ImGui::PushFont(m_uiManager->font_body);
    float text_width = ImGui::CalcTextSize(authorsText).x;
    ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
    ImGui::Text("%s", authorsText);
    ImGui::PopFont();

    ImGui::End();
}

bool CreditsWindow::loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    auto [pData, dataSize] = ServiceManager::getAssetManager()->getImage(filename);
    if (!pData || dataSize == 0) return false;

    // Decode PNG -> RGBA and upload into an OpenGL texture
    int image_width = 0, image_height = 0;
    unsigned char* image_data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(pData), dataSize, &image_width, &image_height, nullptr, 4);
    if (!image_data) return false;

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture; *out_width = image_width; *out_height = image_height;
    return true;
}