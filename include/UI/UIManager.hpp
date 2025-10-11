#pragma once
#include <memory>
#include <vector>

#include "UIElement.hpp"

class UiManager {
public:
    UiManager();
    ~UiManager();
    void startMainLoop();
    void draw();
    void addElement(std::unique_ptr<UIElement> uiElement);
    void removeElement(UIElement* uiElement);
private:
    void showDockingSpace();
    void* m_window;
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif
    std::vector<std::unique_ptr<UIElement>> m_uiElements;
};