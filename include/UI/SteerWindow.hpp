/**
 * @file SteerWindow.hpp
 * @brief "Steering wheel" window: steering wheel image rotated by the steering angle.
 *
 * @details
 * Channel used: `steer` (degrees; positive = clockwise rotation of the image).
 * The `steering_wheel.png` image is loaded through the asset manager.
 */
#pragma once
#include <string>
#include <mutex>
#include <GLFW/glfw3.h>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

class UiManager;

/**
 * @brief Animated steering wheel with indicator and numeric angle.
 */
class SteerWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @brief Loads the steering wheel texture. @param manager Owning UiManager. */
    explicit SteerWindow(UiManager* manager);
    /** @brief Releases the OpenGL texture. */
    ~SteerWindow() override;

    /** @brief Draws the rotated wheel, the fixed reference, the red indicator and the angle. */
    void draw() override;
    /** @brief Stores the steering angle (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    /**
     * @brief Loads an image from a file on disk into a texture (not used: the in-memory version is used).
     * @return true on success.
     */
    bool loadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
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
    std::mutex m_dataMutex;   ///< Protects m_steeringAngle.

    float m_steeringAngle = 0.0f;  ///< Latest live angle (degrees).

    GLuint m_wheelTextureID = 0;  ///< Steering wheel texture (0 if not loaded).
    int m_wheelWidth = 0;         ///< Image width (px).
    int m_wheelHeight = 0;        ///< Image height (px).
};