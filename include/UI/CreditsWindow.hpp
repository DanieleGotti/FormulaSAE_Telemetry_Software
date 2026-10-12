/**
 * @file CreditsWindow.hpp
 * @brief "Credits" window: team logo and authors, visible only on the Home screen.
 */
#pragma once
#include <string>
#include <GLFW/glfw3.h>
#include "UIElement.hpp"

class UiManager;

/**
 * @brief Centered ERB logo with the list of authors (CONFIGURING state only).
 */
class CreditsWindow : public UIElement {
public:
    /** @brief Loads the logo texture (`logo_erb.png`). @param manager Owning UiManager. */
    explicit CreditsWindow(UiManager* manager);
    /** @brief Releases the OpenGL texture. */
    ~CreditsWindow() override;

    /** @brief Draws logo and authors, centered (does nothing outside the Home screen). */
    void draw() override;

private:
    /**
     * @brief Loads an image through the asset manager into an OpenGL texture.
     * @param filename         Image name.
     * @param[out] out_texture Texture ID.
     * @param[out] out_width   Width (px).
     * @param[out] out_height  Height (px).
     * @return true on success.
     */
    bool loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height);

    UiManager* m_uiManager;   ///< Owner.
    GLuint m_textureID = 0;   ///< Logo texture (0 if not loaded).
    int m_imgWidth = 0;       ///< Image width (px).
    int m_imgHeight = 0;      ///< Image height (px).
    
    const float TARGET_IMAGE_WIDTH = 250.0f;  ///< Width the logo is drawn at (px).
};