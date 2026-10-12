/**
 * @file UIElement.hpp
 * @brief Base interface of every window of the interface.
 */
#pragma once

/**
 * @brief Any drawable element managed by UiManager.
 */
class UIElement {
public:
    virtual ~UIElement() = default;
    
    /**
     * @brief Draws the element with ImGui. Called every frame from the UI thread.
     *
     * Typically: `ImGui::Begin("Title") ... ImGui::End()`.
     */
    virtual void draw() = 0;
};