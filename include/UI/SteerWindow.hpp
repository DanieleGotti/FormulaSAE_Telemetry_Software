#pragma once
#include <string>
#include <mutex>
#include <GLFW/glfw3.h>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

class UiManager;

class SteerWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit SteerWindow(UiManager* manager);
    ~SteerWindow() override;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    bool loadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
    bool loadTextureFromMemory(const char* filename, GLuint* out_texture, int* out_width, int* out_height);

    UiManager* m_uiManager;
    std::mutex m_dataMutex;

    float m_steeringAngle = 0.0f;

    GLuint m_wheelTextureID = 0;
    int m_wheelWidth = 0;
    int m_wheelHeight = 0;
};