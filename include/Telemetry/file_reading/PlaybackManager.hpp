#pragma once
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <atomic>
#include <thread>
#include "Telemetry/data_writing/DataAggregator.hpp" 

// Native C++ struct for maximum DOD performance (contiguous memory, no strings)
struct NativeTelemetryRow {
    double timestamp = 0.0;
    
    // 0xCC (5ms) fields
    float front_left_velocity = 0.0f, front_right_velocity = 0.0f, rear_left_velocity = 0.0f, rear_right_velocity = 0.0f;
    float front_left_suspension = 0.0f, front_right_suspension = 0.0f;
    float accelerator1 = 0.0f, accelerator2 = 0.0f, accelerator_mapped = 0.0f, brake1 = 0.0f, brake2 = 0.0f, steer = 0.0f;
    int sdc = 0, ready_to_drive_button = 0, ecu_reset_button = 0, tractive_system_on_button = 0;
    float emma_current = 0.0f, emma_voltage = 0.0f, emma_yaw = 0.0f;
    uint16_t emma_error = 0;
    float mean_velocity = 0.0f, real_yaw_rate = 0.0f, total_torque_request = 0.0f;
    float torque_tv_L = 0.0f, torque_tv_R = 0.0f, slip_L = 0.0f, slip_R = 0.0f;
    float torque_reduction_L = 0.0f, torque_reduction_R = 0.0f;
    float final_torque_target_L = 0.0f, final_torque_target_R = 0.0f;
    float inv_L_torqueCurrent = 0.0f, inv_L_magnetizingCurrent = 0.0f, inv_L_tempMotor = 0.0f;
    float inv_R_torqueCurrent = 0.0f, inv_R_magnetizingCurrent = 0.0f, inv_R_tempMotor = 0.0f;
    float left_coolant_temp = 0.0f, right_coolant_temp = 0.0f;
    uint8_t left_inverter_fsm = 0, right_inverter_fsm = 0, tractive_system_fsm = 0, ECU_Mode = 0;
    uint16_t IMDInsulationValue = 0; // kOhm

    // 0xEE (200ms) fields
    uint8_t state_of_charge = 0;
    float voltages[14] = {0};
    float temperatures1[14] = {0};
    float temperatures2[14] = {0};
    uint16_t online_modules = 0;
    
    // Pre-calculated fast running statistics (3 seconds window)
    float acc1_mean = 0.0f, acc1_var = 0.0f, acc1_std = 0.0f;
    float acc2_mean = 0.0f, acc2_var = 0.0f, acc2_std = 0.0f;
    float brk1_mean = 0.0f, brk1_var = 0.0f, brk1_std = 0.0f;
    float brk2_mean = 0.0f, brk2_var = 0.0f, brk2_std = 0.0f;
    float steer_mean = 0.0f, steer_var = 0.0f, steer_std = 0.0f;

    // Session-cumulative max/mean (same semantics as the live DataAggregator)
    float speed_max = 0.0f, speed_mean = 0.0f;
    float inv_L_tempMotor_max = 0.0f, inv_L_tempMotor_mean = 0.0f;
    float inv_R_tempMotor_max = 0.0f, inv_R_tempMotor_mean = 0.0f;
    float left_coolant_temp_max = 0.0f, left_coolant_temp_mean = 0.0f;
    float right_coolant_temp_max = 0.0f, right_coolant_temp_mean = 0.0f;

    // Lost packets tracking
    uint32_t lostA = 0, lostB = 0, lostTot = 0;
};

class PlaybackManager {
public:
    PlaybackManager() = default;
    ~PlaybackManager();

    // Load native data and prepare decimation 
    void setNativeData(std::vector<NativeTelemetryRow>&& data, const std::string& originalFileExt);
    void clearData();

    // Timeline navigation (operates on the 10Hz low-res buffer for UI performance)
    void setCurrentIndex(size_t index);
    size_t getCurrentIndex() const;
    size_t getTotalRows() const;

    // Generates a traditional DbRow Just-In-Time (JIT) to ensure backward compatibility with UI components
    std::optional<DbRow> getCurrentRow() const;
    std::optional<DbRow> getRowAtIndex(size_t index) const;

    // Ultra-fast data extraction for ImPlot (O(N) contiguous memory reads)
    std::vector<double> getPlotTimeline() const;
    std::vector<double> getPlotData(const std::string& key) const;

    // High-Res CSV Export capabilities
    bool canGenerateCsv() const;
    void generateCsvFromBin(const std::string& outputPath);
    bool isExportingCsv() const; // New: to check status for UI
    void cancelExport();         // New: to stop safely


private:
    DbRow convertNativeToDbRow(const NativeTelemetryRow& row) const;
    static std::string formatDouble(double val, int dec);
    static std::string getEmmaErrorStr(uint16_t error);
    static std::string getInverterFsmStr(uint8_t val);
    static std::string getTractiveFsmStr(uint8_t val);
    static std::string getEcuModeStr(uint8_t val);

    std::vector<NativeTelemetryRow> m_highResData; // 200 Hz for accurate calculations and CSV export
    std::vector<NativeTelemetryRow> m_lowResData;  // 10 Hz for smooth 60FPS UI Plotting
    
    std::string m_originalFileExt;
    size_t m_currentIndex = 0;
    mutable std::mutex m_dataMutex;

        // --- NEW: Thread safety and Async Export ---
    std::atomic<bool> m_isExporting{false};
    std::atomic<bool> m_cancelExport{false};
    std::thread m_exportThread;
};