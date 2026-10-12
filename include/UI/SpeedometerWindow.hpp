/**
 * @file SpeedometerWindow.hpp
 * @brief "Speedometer" window: analog speedometer 0-180 km/h.
 *
 * @details
 * The speed shown is the mean of `rear_left_velocity` and `rear_right_velocity`
 * (the same definition of "speed" used by the statistics).
 */
#pragma once
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager;

/**
 * @brief Speedometer drawn with the ImDrawList primitives.
 */
class SpeedometerWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit SpeedometerWindow(UiManager* manager);
    ~SpeedometerWindow() override = default;

    /** @brief Draws dial, ticks, speed arc, needle and numeric value. */
    void draw() override;
    /** @brief Computes the mean speed of the rear wheels (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;          ///< Owner.
    std::mutex m_dataMutex;          ///< Protects m_rearWheelsSpeed.
    float m_rearWheelsSpeed = 0.0f;  ///< Latest live speed (km/h).
};