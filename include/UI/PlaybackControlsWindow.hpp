/**
 * @file PlaybackControlsWindow.hpp
 * @brief "Playback control" window: time slider and arrows to scroll through a loaded session.
 *
 * @details
 * It exists only in CONNECTED_PLAYBACK. It moves the current index of the
 * PlaybackManager (on the 10 Hz copy: every step = 100 ms); every other window
 * then reads the current row from there.
 */
#pragma once
#include "UI/UIElement.hpp"

class UiManager;

/**
 * @brief Time navigation controls of the playback.
 */
class PlaybackControlsWindow : public UIElement {
public:
    /** @param manager Owning UiManager. */
    explicit PlaybackControlsWindow(UiManager* manager);
    /** @brief Draws slider, back/forward arrows and current timestamp. */
    void draw() override;
    
private:
    UiManager* m_uiManager;      ///< Owner.
    int m_sliderPosition = 0;    ///< Slider position (synchronised with the PlaybackManager every frame).
};