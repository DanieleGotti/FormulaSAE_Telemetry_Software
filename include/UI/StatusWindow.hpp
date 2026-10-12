/**
 * @file StatusWindow.hpp
 * @brief "Status panel" window: digital inputs (LEDs), state machines, ECU mode and state of charge.
 *
 * @details
 * Channels used: `sdc`, `ready_to_drive_button`, `ecu_reset_button`,
 * `tractive_system_on_button`, `left_inverter_fsm`, `right_inverter_fsm`,
 * `tractive_system_fsm`, `ECU_Mode`, `state_of_charge`.
 * FSM states already arrive as names (see TelemetrySchema) and are shown with
 * their numeric code in front, e.g. "(5) ENABLED".
 */
#pragma once
#include <string>
#include <mutex>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"
#include "../Telemetry/data_writing/DataAggregator.hpp"

class UiManager; 

/**
 * @brief Vehicle status panel.
 */
class StatusWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit StatusWindow(UiManager* manager);
    ~StatusWindow() override = default;

    /** @brief Draws the three-column table: LEDs, FSMs, battery. */
    void draw() override;
    /** @brief Extracts and stores the status values from the live row (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    /**
     * @brief Draws a round LED, green (true) or red (false), followed by a label.
     * @param label Text next to the LED.
     * @param state Input state.
     */
    void drawLed(const char* label, bool state);
    /**
     * @brief Draws the battery icon filled according to the state of charge.
     * @param soc State of charge in percent (green > 50, orange > 20, red otherwise).
     */
    void drawBattery(int soc);
    
    float m_leftPaneWidth = 280.0f;  ///< Not used.

    UiManager* m_uiManager;   ///< Owner.
    std::mutex m_dataMutex;   ///< Protects the values below.

    // Latest live values received
    bool m_r2dButtonState = false;            ///< Ready To Drive button pressed.
    bool m_resetButtonState = false;          ///< ECU reset button pressed.
    bool m_sdcInputState = false;             ///< Shutdown circuit closed.
    bool m_tsOnButtonState = false;           ///< Tractive System ON button pressed.
    std::string m_leftInverterFsm = "NULL";   ///< Left inverter FSM state.
    std::string m_rightInverterFsm = "NULL";  ///< Right inverter FSM state.
    std::string m_tractiveSystemFsm = "NULL"; ///< Tractive system FSM state.
    std::string m_ecuMode = "NULL";           ///< ECU mode.
    int m_stateOfCharge = 0;                  ///< State of charge (%).
};