#include "UI/CreditsWindow.hpp"
#include "UI/UIManager.hpp"
#include "utils/IAssetManager.hpp"
#include "Telemetry/Services/ServiceManager.hpp"
#include "../external/stb_image/stb_image.h" 
#include <imgui.h>
#include <iostream>

CreditsWindow::CreditsWindow(UiManager* manager) : m_uiManager(manager) {
    if (!loadTextureFromMemory("logo_erb.png", &m_textureID, &m_imgWidth, &m_imgHeight)) {
        std::cerr << "ERROR [CreditsWindow]: Failed to load image 'logo_erb.png'." << std::endl;
    }
}

CreditsWindow::~CreditsWindow() {
    if (m_textureID) glDeleteTextures(1, &m_textureID);
}

void CreditsWindow::draw() {
    if (m_uiManager->getCurrentState() != AppState::CONFIGURING) return;

    // Definiamo una dimensione per la finestra (es. 500x400)
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    // Rimosso AlwaysAutoResize!
    ImGui::Begin("Credits", nullptr, ImGuiWindowFlags_NoCollapse);

    float window_width = ImGui::GetWindowSize().x;
    float window_height = ImGui::GetWindowSize().y;
    const char* authorsText = "Authors:      Daniele Gotti      Samuele Stasi"; 

    // --- 1. PRE-CALCOLIAMO L'ALTEZZA DEL CONTENUTO ---
    float target_height = 0.0f;
    if (m_textureID && m_imgWidth > 0) {
        float aspect_ratio = static_cast<float>(m_imgHeight) / static_cast<float>(m_imgWidth);
        target_height = TARGET_IMAGE_WIDTH * aspect_ratio;
    }

    ImGui::PushFont(m_uiManager->font_body);
    float text_height = ImGui::CalcTextSize(authorsText).y;
    ImGui::PopFont();

    // Altezza totale = Immagine + Spazio Vuoto + Testo
    float spacing = ImGui::GetStyle().ItemSpacing.y; // Equivalente a ImGui::Spacing()
    float total_content_height = target_height + spacing + text_height;

    // --- 2. CENTRIAMO VERTICALMENTE ---
    // Spostiamo il cursore Y in modo da avere margini uguali sopra e sotto
    float start_y = (window_height - total_content_height) * 0.5f;
    if (start_y > 0) {
        ImGui::SetCursorPosY(start_y); // Evita valori negativi se la finestra è troppo piccola
    }

    // --- 3. DISEGNAMO (con il tuo centraggio orizzontale) ---
    if (m_textureID && m_imgWidth > 0) {
        ImGui::SetCursorPosX((window_width - TARGET_IMAGE_WIDTH) * 0.5f);
        ImGui::Image((void*)(intptr_t)m_textureID, ImVec2(TARGET_IMAGE_WIDTH, target_height));
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacing); // Aggiungiamo lo spazio

    ImGui::Dummy(ImVec2(0.0f, 20.0f)); 

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