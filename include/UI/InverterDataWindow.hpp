/**
 * @file InverterDataWindow.hpp
 * @brief "Inverter and cooling data" window: inverter currents and temperatures, cooling, AMK diagnostics.
 *
 * @details
 * Channels used: `inv_L/R_torqueCurrent`, `inv_L/R_magnetizingCurrent`,
 * `inv_L/R_tempMotor`, `left/right_coolant_temp`, `inv_L/R_errorInfo`.
 * Temperatures are coloured with thresholds (green <= 100 °C, red >= 120 °C,
 * orange in between). The AMK diagnostic is green when absent, yellow for a
 * warning, red for an error (see AmkDiagnostics.hpp).
 */
#pragma once
#include <mutex>
#include <map>
#include <string>
#include <imgui.h> 
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp" 

class UiManager; 

/**
 * @brief List of inverter values with coloured thresholds.
 */
class InverterDataWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit InverterDataWindow(UiManager* manager); 
    
    /** @brief Draws the sections (currents, temperatures, pumps, diagnostics). */
    void draw() override;
    /** @brief Stores the latest live row (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;
    
private:
    /**
     * @brief Prints "Label:  value [unit]" with value and unit aligned to the right.
     * @param label   Label on the left.
     * @param key     Field name in the row.
     * @param dataMap Row to read from ("Null" when the key is missing).
     * @param unit    Unit of measure ("" -> "[-]").
     */
    void printValue(const std::string& label, const std::string& key, const DbRow& dataMap, const std::string& unit = "");
    /**
     * @brief Like printValue() but with the value coloured by thresholds and a fixed "[°C]" unit.
     * @param label      Label.
     * @param key        Field name.
     * @param dataMap    Row to read from.
     * @param limitGreen Up to this value: green.
     * @param limitRed   From this value: red (with a warning tooltip).
     */
    void printColoredValue(const std::string& label, const std::string& key, const DbRow& dataMap, float limitGreen, float limitRed);
    /**
     * @brief Colour of a value with respect to the thresholds.
     * @return Green if value <= limitGreen, red if value >= limitRed, orange otherwise.
     */
    ImVec4 getColorForThreshold(float value, float limitGreen, float limitRed);

    UiManager* m_uiManager;                            ///< Owner.
    std::map<std::string, std::string> m_latestData;   ///< Latest live row received.
    std::mutex m_dataMutex;                            ///< Protects m_latestData.
};