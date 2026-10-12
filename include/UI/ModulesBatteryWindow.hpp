/**
 * @file ModulesBatteryWindow.hpp
 * @brief "Battery (EMMA)" window: EMMA BMS status and grid of the 14 battery modules.
 *
 * @details
 * Channels used: `emma_current`, `emma_voltage`, `emma_yaw`, `emma_error` and,
 * for every module N (1..14): `Temperature1ModuleN`, `Temperature2ModuleN`,
 * `VoltageModuleN`, `OnlineModuleN` (the latter derived from `online_modules`,
 * see TelemetrySchema::expandDerivedKeys()).
 * Colour thresholds: temperature green < 50 °C, orange < 65 °C, red above;
 * voltage green between 3.6 and 4.2 V, orange between 3.0 and 4.5 V, red outside.
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <imgui.h>
#include "UIElement.hpp"
#include "../Telemetry/data_writing/IAggregatedDataSubscriber.hpp"

class UiManager;

/**
 * @brief EMMA status and battery module cards.
 */
class ModulesBatteryWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit ModulesBatteryWindow(UiManager* manager);
    ~ModulesBatteryWindow() override = default;

    /** @brief Draws the EMMA status and the module grid (columns adapted to the width). */
    void draw() override;
    /** @brief Stores the latest live row (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;

private:
    UiManager* m_uiManager;                            ///< Owner.
    std::mutex m_dataMutex;                            ///< Protects m_latestData.
    std::map<std::string, std::string> m_latestData;   ///< Latest live row received.

    /**
     * @brief Draws the card of one module: two temperatures, voltage and "online" LED.
     * @param index   Module number (1..14).
     * @param dataMap Row to read from.
     */
    void drawModuleCard(int index, const DbRow& dataMap);
    
    /** @brief Colour of a module temperature (50 / 65 °C thresholds). */
    ImVec4 getTempColor(float value);
    /** @brief Colour of a module voltage (ok 3.6-4.2 V, warning 3.0-4.5 V). */
    ImVec4 getVoltageColor(float value);
    /**
     * @brief Prints the `emma_error` field with its hex code; green if OK or precharge finished, red otherwise.
     * @param label   Label.
     * @param dataMap Row to read from.
     */
    void printErrorValue(const std::string& label, const DbRow& dataMap);
    /**
     * @brief Prints "Label:  value [unit]" with value and unit aligned to the right.
     * @param label   Label on the left.
     * @param key     Field name in the row.
     * @param dataMap Row to read from ("Null" when the key is missing).
     * @param unit    Unit of measure ("" -> "[-]").
     */
    void printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit = "");
};