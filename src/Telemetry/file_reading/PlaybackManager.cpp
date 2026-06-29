#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>

void PlaybackManager::setNativeData(std::vector<NativeTelemetryRow>&& data, const std::string& originalFileExt) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_highResData = std::move(data);
    m_originalFileExt = originalFileExt;
    m_currentIndex = 0;

    m_lowResData.clear();
    if (!m_highResData.empty()) {
        m_lowResData.reserve(m_highResData.size() / 20 + 1);
        for (size_t i = 0; i < m_highResData.size(); i += 20) {
            m_lowResData.push_back(m_highResData[i]);
        }
    }
}

void PlaybackManager::clearData() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_highResData.clear();
    m_lowResData.clear();
    m_currentIndex = 0;
}

void PlaybackManager::setCurrentIndex(size_t index) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (index < m_lowResData.size()) {
        m_currentIndex = index;
    }
}

size_t PlaybackManager::getCurrentIndex() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_currentIndex;
}

size_t PlaybackManager::getTotalRows() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_lowResData.size();
}

std::optional<DbRow> PlaybackManager::getCurrentRow() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_lowResData.empty() && m_currentIndex < m_lowResData.size()) {
        return convertNativeToDbRow(m_lowResData[m_currentIndex]);
    }
    return std::nullopt;
}

std::optional<DbRow> PlaybackManager::getRowAtIndex(size_t index) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_lowResData.empty() && index < m_lowResData.size()) {
        return convertNativeToDbRow(m_lowResData[index]);
    }
    return std::nullopt;
}

std::vector<double> PlaybackManager::getPlotTimeline() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<double> timeline(m_lowResData.size());
    for (size_t i = 0; i < m_lowResData.size(); ++i) {
        timeline[i] = m_lowResData[i].timestamp;
    }
    return timeline;
}

std::vector<double> PlaybackManager::getPlotData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<double> data(m_lowResData.size(), 0.0);
    
    for (size_t i = 0; i < m_lowResData.size(); ++i) {
        const auto& r = m_lowResData[i];
        if (key == "accelerator1") data[i] = r.accelerator1;
        else if (key == "accelerator2") data[i] = r.accelerator2;
        else if (key == "accelerator_mapped") data[i] = r.accelerator_mapped;
        else if (key == "brake1") data[i] = r.brake1;
        else if (key == "brake2") data[i] = r.brake2;
        else if (key == "steer") data[i] = r.steer;
        else if (key == "front_left_suspension") data[i] = r.front_left_suspension;
        else if (key == "front_right_suspension") data[i] = r.front_right_suspension;
        else if (key == "front_left_velocity") data[i] = r.front_left_velocity;
        else if (key == "front_right_velocity") data[i] = r.front_right_velocity;
        else if (key == "rear_left_velocity") data[i] = r.rear_left_velocity;
        else if (key == "rear_right_velocity") data[i] = r.rear_right_velocity;
    }
    return data;
}

bool PlaybackManager::canGenerateCsv() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_originalFileExt == ".bin" && !m_highResData.empty();
}

PlaybackManager::~PlaybackManager() {
    cancelExport();
}

bool PlaybackManager::isExportingCsv() const {
    return m_isExporting.load();
}

void PlaybackManager::cancelExport() {
    if (m_isExporting.load()) {
        m_cancelExport.store(true);
        if (m_exportThread.joinable()) {
            m_exportThread.join();
        }
    }
}

// Thread-safe and Crash-free CSV Exporter 
void PlaybackManager::generateCsvFromBin(const std::string& outputPath) {
    if (m_isExporting.load()) return; 

    // FIX CRASH: Cleanly join any previous finished thread before spawning a new one
    if (m_exportThread.joinable()) {
        m_exportThread.join();
    }

    m_isExporting.store(true);
    m_cancelExport.store(false);

    m_exportThread = std::thread([this, outputPath]() {
        std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "ERROR [PlaybackManager]: Could not open file for CSV export." << std::endl;
            m_isExporting.store(false);
            return;
        }

        std::vector<NativeTelemetryRow> localData;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            localData = m_highResData; 
        }

        // Write Hardcoded Header identical to the Live Logger
        out << "timestamp;front_left_velocity;front_right_velocity;rear_left_velocity;rear_right_velocity;"
            << "front_left_suspension;front_right_suspension;accelerator1;accelerator2;accelerator_mapped;"
            << "brake1;brake2;steer;sdc;ready_to_drive_button;ecu_reset_button;tractive_system_on_button;"
            << "emma_current;emma_voltage;emma_yaw;emma_error;mean_velocity;real_yaw_rate;total_torque_request;"
            << "torque_tv_L;torque_tv_R;slip_L;slip_R;torque_reduction_L;torque_reduction_R;final_torque_target_L;final_torque_target_R;"
            << "inv_L_torqueCurrent;inv_L_magnetizingCurrent;inv_L_tempMotor;inv_R_torqueCurrent;inv_R_magnetizingCurrent;inv_R_tempMotor;"
            << "left_coolant_temp;right_coolant_temp;left_inverter_fsm;right_inverter_fsm;tractive_system_fsm;ECU_Mode;state_of_charge;";
        
        for (int i = 1; i <= 14; ++i) out << "VoltageModule" << i << ";";
        for (int i = 1; i <= 14; ++i) out << "Temperature1Module" << i << ";Temperature2Module" << i << ";";
        
        out << "accelerator1_mean;accelerator1_var;accelerator1_std;"
            << "accelerator2_mean;accelerator2_var;accelerator2_std;"
            << "brake1_mean;brake1_var;brake1_std;"
            << "brake2_mean;brake2_var;brake2_std;"
            << "steer_mean;steer_var;steer_std;"
            << "lost_packets_A;lost_packets_B;lost_packets_tot\n";

        // Write Hybrid Data: High-speed Numerics + String State Machines to perfectly match Live CSV formatting
        for (const auto& row : localData) {
            if (m_cancelExport.load()) break;

            out << std::fixed << std::setprecision(3) << row.timestamp << ";"
                << std::setprecision(1) 
                << row.front_left_velocity << ";" << row.front_right_velocity << ";" 
                << row.rear_left_velocity << ";" << row.rear_right_velocity << ";"
                << row.front_left_suspension << ";" << row.front_right_suspension << ";"
                << row.accelerator1 << ";" << row.accelerator2 << ";" << row.accelerator_mapped << ";"
                << row.brake1 << ";" << row.brake2 << ";" << row.steer << ";"
                << row.sdc << ";" << row.ready_to_drive_button << ";" << row.ecu_reset_button << ";" << row.tractive_system_on_button << ";"
                << row.emma_current << ";" << row.emma_voltage << ";" 
                << std::setprecision(2) << row.emma_yaw << ";"
                << PlaybackManager::getEmmaErrorStr(row.emma_error) << ";" // Converted to String
                << std::setprecision(1) << row.mean_velocity << ";" 
                << std::setprecision(2) << row.real_yaw_rate << ";" << row.total_torque_request << ";"
                << row.torque_tv_L << ";" << row.torque_tv_R << ";" 
                << std::setprecision(3) << row.slip_L << ";" << row.slip_R << ";" 
                << std::setprecision(2) << row.torque_reduction_L << ";" << row.torque_reduction_R << ";" 
                << row.final_torque_target_L << ";" << row.final_torque_target_R << ";"
                << std::setprecision(1) 
                << row.inv_L_torqueCurrent << ";" << row.inv_L_magnetizingCurrent << ";" << row.inv_L_tempMotor << ";"
                << row.inv_R_torqueCurrent << ";" << row.inv_R_magnetizingCurrent << ";" << row.inv_R_tempMotor << ";"
                << row.left_coolant_temp << ";" << row.right_coolant_temp << ";"
                << PlaybackManager::getInverterFsmStr(row.left_inverter_fsm) << ";" // Converted to String
                << PlaybackManager::getInverterFsmStr(row.right_inverter_fsm) << ";" 
                << PlaybackManager::getTractiveFsmStr(row.tractive_system_fsm) << ";" 
                << PlaybackManager::getEcuModeStr(row.ECU_Mode) << ";"
                << (int)row.state_of_charge << ";";

            for (int i = 0; i < 14; ++i) out << std::setprecision(2) << row.voltages[i] << ";";
            for (int i = 0; i < 14; ++i) out << std::setprecision(1) << row.temperatures1[i] << ";" << row.temperatures2[i] << ";";

            out << std::setprecision(3)
                << row.acc1_mean << ";" << row.acc1_var << ";" << row.acc1_std << ";"
                << row.acc2_mean << ";" << row.acc2_var << ";" << row.acc2_std << ";"
                << row.brk1_mean << ";" << row.brk1_var << ";" << row.brk1_std << ";"
                << row.brk2_mean << ";" << row.brk2_var << ";" << row.brk2_std << ";"
                << row.steer_mean << ";" << row.steer_var << ";" << row.steer_std << ";"
                << row.lostA << ";" << row.lostB << ";" << row.lostTot << "\n";
        }

        out.close();
        if (!m_cancelExport.load()) {
            std::cout << "INFO [PlaybackManager]: High-Res CSV exported successfully to " << outputPath << std::endl;
        }
        
        m_isExporting.store(false);
    });
}

std::string PlaybackManager::formatDouble(double val, int dec) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(dec) << val;
    return ss.str();
}

std::string PlaybackManager::getEmmaErrorStr(uint16_t error) {
    if (error == 0) return "EMMA_STATUS_OK";
    std::vector<std::string> flags;
    if (error & 0x0001) flags.push_back("ERR_UNDERVOLTAGE");
    if (error & 0x0002) flags.push_back("ERR_OVERVOLTAGE");
    if (error & 0x0004) flags.push_back("ERR_OVERTEMP_CELL");
    if (error & 0x0008) flags.push_back("ERR_UNDERTEMP_CELL");
    if (error & 0x0010) flags.push_back("ERR_OVERTEMP_PCB");
    if (error & 0x0020) flags.push_back("ERR_UNDERTEMP_PCB");
    if (error & 0x0040) flags.push_back("ERR_CELL_MISMATCH");
    if (error & 0x0080) flags.push_back("ERR_BSPD");
    if (error & 0x0100) flags.push_back("ERR_DAISY_CHAIN");
    if (error & 0x0200) flags.push_back("IMD_AMS_LATCH");
    if (error & 0x8000) flags.push_back("ERR_CRITICAL_FAULT");

    std::string res;
    for (size_t i = 0; i < flags.size(); ++i) {
        res += flags[i];
        if (i < flags.size() - 1) res += " | ";
    }
    return res.empty() ? "UNKNOWN" : res;
}

std::string PlaybackManager::getInverterFsmStr(uint8_t val) {
    const char* states[] = { "OFF", "SYSTEM_READY", "DC_CAPACITOR_CHARGE", "DC_OK", "ENABLE_INVERTER", "ENABLED", "ON", "OK", "ERROR" };
    if (val <= 8) return states[val]; return "UNKNOWN";
}

std::string PlaybackManager::getTractiveFsmStr(uint8_t val) {
    const char* states[] = { "ENTRY_STATE", "START_LIGHT_TEST", "WAIT_LIGHT_TEST", "POWER_OFF", "TS_WAIT_ACTIVATION", "TS_WAIT_PRECHARGE", "TS_ACTIVE", "R2D_WAIT", "R2D_ACTIVE", "ERROR_STATE" };
    if (val <= 9) return states[val]; return "UNKNOWN";
}

std::string PlaybackManager::getEcuModeStr(uint8_t val) {
    if (val & 0x01) return "ENDURANCE"; if (val & 0x02) return "ACCELERATION"; if (val & 0x04) return "TEST"; return "UNKNOWN";
}

DbRow PlaybackManager::convertNativeToDbRow(const NativeTelemetryRow& row) const {
    DbRow out;
    out["timestamp"] = formatDouble(row.timestamp, 3);
    
    out["front_left_velocity"] = formatDouble(row.front_left_velocity, 1);
    out["front_right_velocity"] = formatDouble(row.front_right_velocity, 1);
    out["rear_left_velocity"] = formatDouble(row.rear_left_velocity, 1);
    out["rear_right_velocity"] = formatDouble(row.rear_right_velocity, 1);

    out["front_left_suspension"] = formatDouble(row.front_left_suspension, 1);
    out["front_right_suspension"] = formatDouble(row.front_right_suspension, 1);

    out["accelerator1"] = formatDouble(row.accelerator1, 1);
    out["accelerator2"] = formatDouble(row.accelerator2, 1);
    out["accelerator_mapped"] = formatDouble(row.accelerator_mapped, 1);

    out["brake1"] = formatDouble(row.brake1, 1);
    out["brake2"] = formatDouble(row.brake2, 1);
    out["steer"] = formatDouble(row.steer, 1);

    out["sdc"] = std::to_string(row.sdc);
    out["ready_to_drive_button"] = std::to_string(row.ready_to_drive_button);
    out["ecu_reset_button"] = std::to_string(row.ecu_reset_button);
    out["tractive_system_on_button"] = std::to_string(row.tractive_system_on_button);

    out["emma_current"] = formatDouble(row.emma_current, 1);
    out["emma_voltage"] = formatDouble(row.emma_voltage, 1);
    out["emma_yaw"] = formatDouble(row.emma_yaw, 2);
    out["emma_error"] = getEmmaErrorStr(row.emma_error);

    out["mean_velocity"] = formatDouble(row.mean_velocity, 1);
    out["real_yaw_rate"] = formatDouble(row.real_yaw_rate, 2);
    out["total_torque_request"] = formatDouble(row.total_torque_request, 2);
    out["torque_tv_L"] = formatDouble(row.torque_tv_L, 2);
    out["torque_tv_R"] = formatDouble(row.torque_tv_R, 2);
    out["slip_L"] = formatDouble(row.slip_L, 3);
    out["slip_R"] = formatDouble(row.slip_R, 3);
    out["torque_reduction_L"] = formatDouble(row.torque_reduction_L, 2);
    out["torque_reduction_R"] = formatDouble(row.torque_reduction_R, 2);
    out["final_torque_target_L"] = formatDouble(row.final_torque_target_L, 2);
    out["final_torque_target_R"] = formatDouble(row.final_torque_target_R, 2);

    out["inv_L_torqueCurrent"] = formatDouble(row.inv_L_torqueCurrent, 1);
    out["inv_L_magnetizingCurrent"] = formatDouble(row.inv_L_magnetizingCurrent, 1);
    out["inv_L_tempMotor"] = formatDouble(row.inv_L_tempMotor, 1);
    out["inv_R_torqueCurrent"] = formatDouble(row.inv_R_torqueCurrent, 1);
    out["inv_R_magnetizingCurrent"] = formatDouble(row.inv_R_magnetizingCurrent, 1);
    out["inv_R_tempMotor"] = formatDouble(row.inv_R_tempMotor, 1);

    out["left_coolant_temp"] = formatDouble(row.left_coolant_temp, 1);
    out["right_coolant_temp"] = formatDouble(row.right_coolant_temp, 1);

    out["left_inverter_fsm"] = getInverterFsmStr(row.left_inverter_fsm);
    out["right_inverter_fsm"] = getInverterFsmStr(row.right_inverter_fsm);
    out["tractive_system_fsm"] = getTractiveFsmStr(row.tractive_system_fsm);
    out["ECU_Mode"] = getEcuModeStr(row.ECU_Mode);

    out["state_of_charge"] = std::to_string(row.state_of_charge);
    for(int i=0; i<14; ++i) {
        out["VoltageModule" + std::to_string(i+1)] = formatDouble(row.voltages[i], 2);
        out["Temperature1Module" + std::to_string(i+1)] = formatDouble(row.temperatures1[i], 1);
        out["Temperature2Module" + std::to_string(i+1)] = formatDouble(row.temperatures2[i], 1);
    }

    out["accelerator1_mean"] = formatDouble(row.acc1_mean, 3); out["accelerator1_var"] = formatDouble(row.acc1_var, 3); out["accelerator1_std"] = formatDouble(row.acc1_std, 3);
    out["accelerator2_mean"] = formatDouble(row.acc2_mean, 3); out["accelerator2_var"] = formatDouble(row.acc2_var, 3); out["accelerator2_std"] = formatDouble(row.acc2_std, 3);
    out["brake1_mean"] = formatDouble(row.brk1_mean, 3); out["brake1_var"] = formatDouble(row.brk1_var, 3); out["brake1_std"] = formatDouble(row.brk1_std, 3);
    out["brake2_mean"] = formatDouble(row.brk2_mean, 3); out["brake2_var"] = formatDouble(row.brk2_var, 3); out["brake2_std"] = formatDouble(row.brk2_std, 3);
    out["steer_mean"] = formatDouble(row.steer_mean, 3); out["steer_var"] = formatDouble(row.steer_var, 3); out["steer_std"] = formatDouble(row.steer_std, 3);

    out["lost_packets_A"] = std::to_string(row.lostA);
    out["lost_packets_B"] = std::to_string(row.lostB);
    out["lost_packets_tot"] = std::to_string(row.lostTot);

    return out;
}