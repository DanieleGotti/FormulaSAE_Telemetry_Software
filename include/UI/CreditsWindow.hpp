#pragma once
#include <string>
#include <GLFW/glfw3.h>
#include "UIElement.hpp"

class UiManager;

class CreditsWindow : public UIElement {
public:
    explicit CreditsWindow(UiManager* manager);
    ~CreditsWindow() override;

    void draw() override;

private:
    bool loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height);

    UiManager* m_uiManager;
    GLuint m_textureID = 0;
    int m_imgWidth = 0;
    int m_imgHeight = 0;
    
    // Decidi tu qui la larghezza dell'immagine (in pixel)
    const float TARGET_IMAGE_WIDTH = 250.0f; 
};