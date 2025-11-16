#pragma once
#include "UI/UIElement.hpp"

class UiManager;

class PlaybackControlsWindow : public UIElement {
public:
    explicit PlaybackControlsWindow(UiManager* manager);
    void draw() override;
    
private:
    UiManager* m_uiManager;
    int m_sliderPosition = 0;
};