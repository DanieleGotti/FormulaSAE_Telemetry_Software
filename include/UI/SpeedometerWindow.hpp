#pragma once
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager;

class SpeedometerWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    explicit SpeedometerWindow(UiManager* manager);
    ~SpeedometerWindow() override = default;

    void draw() override;
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;
    std::mutex m_dataMutex;
    float m_meanVelocity = 0.0f;
};