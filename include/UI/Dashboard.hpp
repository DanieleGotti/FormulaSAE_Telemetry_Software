/**
 * @file Dashboard.hpp
 * @brief "Data management" window: start/stop logging, CSV export, lost packets and a table of ALL values.
 *
 * @details
 * - Live: "Start/Stop logging" buttons (CsvWriter in `output_data/`).
 * - Playback: high-resolution CSV export button (only for .bin sources).
 * - Lost packets table (0xCC, 0xEE, total).
 * - "Current data" table: every schema column in canonical order, plus any
 *   unknown column coming from recordings of other versions.
 *   A new field added in TelemetrySchema shows up here automatically.
 */
#pragma once
#include <mutex>
#include <map>
#include <string>
#include "UI/UIElement.hpp"
#include "Telemetry/data_writing/IAggregatedDataSubscriber.hpp" 
#include "Telemetry/data_writing/DataAggregator.hpp" 

class UiManager; 

/**
 * @brief Data management panel and general table of the current values.
 */
class Dashboard : public UIElement, public IAggregatedDataSubscriber {
public:
    /** @param manager Owning UiManager. */
    explicit Dashboard(UiManager* manager);
    
    /** @brief Draws the logging/export controls and the tables. */
    void draw() override;
    /** @brief Stores the latest live row (serial thread). */
    void onAggregatedDataReceived(const DbRow& dataRow) override;
    
private:
    UiManager* m_uiManager;    ///< Owner.
    const std::string OUTPUT_DIRECTORY = "../output_data"; ///< Not used: the real folder is <executable>/output_data.
    std::map<std::string, std::string> m_latestData;  ///< Latest live row received.
    std::mutex m_dataMutex;                           ///< Protects m_latestData.
};