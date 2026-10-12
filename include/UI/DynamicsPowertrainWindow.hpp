/**
 * @file DynamicsPowertrainWindow.hpp
 * @brief "Vehicle dynamics and powertrain targets" window: vehicle dynamics, slip, torque vectoring, traction control.
 *
 * @details
 * Channels used: `mean_velocity`, `real_yaw_rate`, `total_torque_request`,
 * `slip_L/R`, `torque_tv_L/R`, `torque_reduction_L/R`, `final_torque_target_L/R`.
 * It is the simplest example of a text window: to show a new field just add a
 * `printValue("Label", "field_name", dataToDisplay, "unit")` call in draw().
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
 * @brief List of dynamics values and torque targets.
 */
class DynamicsPowertrainWindow : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit DynamicsPowertrainWindow(UiManager* manager); 
    
    /** @brief Draws the value sections. */
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

    UiManager* m_uiManager;                            ///< Owner.
    std::map<std::string, std::string> m_latestData;   ///< Latest live row received.
    std::mutex m_dataMutex;                            ///< Protects m_latestData.
};