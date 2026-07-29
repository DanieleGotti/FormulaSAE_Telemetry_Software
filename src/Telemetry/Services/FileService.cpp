#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdlib>
#include <cstring>

// Fast C-style parsers to avoid std::string allocations and speed up CSV loading
inline float f_atof(const char* s) { return s ? static_cast<float>(std::atof(s)) : 0.0f; }
inline double d_atof(const char* s) { return s ? std::atof(s) : 0.0; }
inline int i_atoi(const char* s) { return s ? std::atoi(s) : 0; }

FileService::FileService() = default;

FileService::~FileService() { 
    // Safely stop background reading if the app is closed to avoid zombies and crashes
    m_cancelRequested.store(true);
    if (m_thread.joinable()) { 
        m_thread.join(); 
    } 
}

bool FileService::configure(const FileConfig& config) { 
    if (m_isLoading) return false; 
    m_config = config; 
    return true; 
}

void FileService::startLoading() { 
    if (m_isLoading || m_isFinished) return;
    m_isLoading = true; 
    m_cancelRequested.store(false); // Reset token for new load
    m_thread = std::thread(&FileService::loadingLoop, this); 
}

bool FileService::isLoading() const { return m_isLoading; } 
bool FileService::isFinished() const { return m_isFinished; }

std::vector<NativeTelemetryRow> FileService::getResult() { 
    std::lock_guard<std::mutex> lock(m_dataMutex); 
    return std::move(m_loadedData); 
}

void FileService::loadingLoop() { 
    std::string filePath = m_config.filePath;
    std::replace(filePath.begin(), filePath.end(), '\\', '/');

    std::string ext = filePath.substr(filePath.find_last_of("."));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".csv") {
        loadCsv(filePath);
    } else if (ext == ".bin") {
        loadBin(filePath);
    } else {
        std::cerr << "ERROR [FileService]: File format not supported." << std::endl;
        m_isLoading = false;
        m_isFinished = true;
    }
}

void FileService::loadBin(const std::string& filePath) { 
    std::ifstream file(filePath, std::ios::binary | std::ios::ate); 
    if (!file.is_open()) { 
        std::cerr << "ERROR [FileService]: Unable to open BIN file." << std::endl; 
        m_isLoading = false; m_isFinished = true; return; 
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    std::vector<NativeTelemetryRow> rawRows;
    rawRows.reserve(fileSize / sizeof(TelemetryPacket_t)); 

    NativeTelemetryRow currentRow = {};
    TelemetryPacket_EMMA_t lastB = {};
    bool hasB = false;

    uint32_t lastTimestampA = 0, lastTimestampB = 0;
    uint32_t lostA = 0, lostB = 0;
    bool firstA = true, firstB = true;

    size_t cursor = 0;
    while (cursor + 4 < buffer.size()) {
        if (m_cancelRequested.load()) {
            std::cout << "WARNING [FileService]: BIN loading aborted by user." << std::endl;
            break; 
        }

        if (buffer[cursor] == 0xCC && buffer[cursor+1] == 0x11 && cursor + sizeof(TelemetryPacket_t) <= buffer.size()) {
            const TelemetryPacket_t* dataA = reinterpret_cast<const TelemetryPacket_t*>(&buffer[cursor]);
            
            if (!firstA && dataA->timestamp > lastTimestampA) {
                uint32_t diff = dataA->timestamp - lastTimestampA;
                if (diff > 1 && diff < 5000) lostA += (diff - 1);
            }
            lastTimestampA = dataA->timestamp;
            firstA = false;

            currentRow.timestamp = dataA->timestamp * 0.005;
            currentRow.front_left_velocity = dataA->front_left_velocity;
            currentRow.front_right_velocity = dataA->front_right_velocity;
            currentRow.rear_left_velocity = dataA->rear_left_velocity;
            currentRow.rear_right_velocity = dataA->rear_right_velocity;
            currentRow.front_left_suspension = dataA->front_left_suspension;
            currentRow.front_right_suspension = dataA->front_right_suspension;
            currentRow.accelerator1 = dataA->accelerator1;
            currentRow.accelerator2 = dataA->accelerator2;
            currentRow.accelerator_mapped = dataA->accelerator_mapped;
            currentRow.brake1 = dataA->brake1;
            currentRow.brake2 = dataA->brake2;
            currentRow.steer = dataA->steer;
            currentRow.sdc = dataA->sdc;
            currentRow.ready_to_drive_button = dataA->ready_to_drive_button;
            currentRow.ecu_reset_button = dataA->ecu_reset_button;
            currentRow.tractive_system_on_button = dataA->tractive_system_on_button;
            currentRow.emma_current = dataA->emma_current;
            currentRow.emma_voltage = dataA->emma_voltage;
            currentRow.emma_yaw = dataA->emma_yaw;
            currentRow.emma_error = dataA->emma_error;
            currentRow.mean_velocity = dataA->mean_velocity;
            currentRow.real_yaw_rate = dataA->real_yaw_rate;
            currentRow.total_torque_request = dataA->total_torque_request;
            currentRow.torque_tv_L = dataA->torque_tv_L;
            currentRow.torque_tv_R = dataA->torque_tv_R;
            currentRow.slip_L = dataA->slip_L;
            currentRow.slip_R = dataA->slip_R;
            currentRow.torque_reduction_L = dataA->torque_reduction_L;
            currentRow.torque_reduction_R = dataA->torque_reduction_R;
            currentRow.final_torque_target_L = dataA->final_torque_target_L;
            currentRow.final_torque_target_R = dataA->final_torque_target_R;
            currentRow.inv_L_torqueCurrent = dataA->inv_L_torqueCurrent / 10.0f;
            currentRow.inv_L_magnetizingCurrent = dataA->inv_L_magnetizingCurrent / 10.0f;
            currentRow.inv_L_tempMotor = dataA->inv_L_tempMotor / 10.0f;
            currentRow.inv_R_torqueCurrent = dataA->inv_R_torqueCurrent / 10.0f;
            currentRow.inv_R_magnetizingCurrent = dataA->inv_R_magnetizingCurrent / 10.0f;
            currentRow.inv_R_tempMotor = dataA->inv_R_tempMotor / 10.0f;
            currentRow.left_coolant_temp = dataA->left_coolant_temp;
            currentRow.right_coolant_temp = dataA->right_coolant_temp;
            currentRow.left_inverter_fsm = dataA->left_inverter_fsm;
            currentRow.right_inverter_fsm = dataA->right_inverter_fsm;
            currentRow.tractive_system_fsm = dataA->tractive_system_fsm;
            currentRow.ECU_Mode = dataA->ECU_Mode;
            
            currentRow.lostA = lostA; currentRow.lostB = lostB; currentRow.lostTot = lostA + lostB;

            if (hasB) {
                currentRow.state_of_charge = lastB.state_of_charge;
                for(int i=0; i<14; ++i) currentRow.voltages[i] = (static_cast<float>(lastB.module_voltage[i]) + 200.0f) / 100.0f;
                for(int i=0; i<28; ++i) {
                    if (i % 2 == 0) currentRow.temperatures1[i/2] = lastB.module_temperatures[i] / 10.0f;
                    else currentRow.temperatures2[i/2] = lastB.module_temperatures[i] / 10.0f;
                }
                currentRow.online_modules = lastB.online_modules;
            }

            rawRows.push_back(currentRow);
            cursor += sizeof(TelemetryPacket_t);
        } 
        else if (buffer[cursor] == 0xEE && buffer[cursor+1] == 0x11 && cursor + sizeof(TelemetryPacket_EMMA_t) <= buffer.size()) {
            const TelemetryPacket_EMMA_t* dataB = reinterpret_cast<const TelemetryPacket_EMMA_t*>(&buffer[cursor]);
            
            if (!firstB && dataB->timestamp > lastTimestampB) {
                uint32_t diff = dataB->timestamp - lastTimestampB;
                if (diff > 40 && diff < 100000) lostB += (diff / 40) - 1;
            }
            lastTimestampB = dataB->timestamp;
            firstB = false;

            lastB = *dataB;
            hasB = true;
            cursor += sizeof(TelemetryPacket_EMMA_t);
        }
        else {
            cursor++;
        }
    }

    const int WINDOW = 600; 
    for (size_t i = 0; i < rawRows.size(); i += WINDOW) {
        if (m_cancelRequested.load()) break; 
        
        size_t end_idx = std::min(i + WINDOW, rawRows.size());
        size_t count = end_idx - i;
        
        double s_a1=0, sq_a1=0, s_a2=0, sq_a2=0, s_b1=0, sq_b1=0, s_b2=0, sq_b2=0, s_st=0, sq_st=0;

        for(size_t j = i; j < end_idx; ++j) {
            s_a1 += rawRows[j].accelerator1; sq_a1 += rawRows[j].accelerator1 * rawRows[j].accelerator1;
            s_a2 += rawRows[j].accelerator2; sq_a2 += rawRows[j].accelerator2 * rawRows[j].accelerator2;
            s_b1 += rawRows[j].brake1;       sq_b1 += rawRows[j].brake1 * rawRows[j].brake1;
            s_b2 += rawRows[j].brake2;       sq_b2 += rawRows[j].brake2 * rawRows[j].brake2;
            s_st += rawRows[j].steer;        sq_st += rawRows[j].steer * rawRows[j].steer;
        }

        auto calc_stats = [&](double sum, double sq, float& m, float& v, float& s) {
            double mean = sum / count;
            double var = (sq / count) - (mean * mean);
            if (var < 0) var = 0;
            m = static_cast<float>(mean); v = static_cast<float>(var); s = static_cast<float>(std::sqrt(var));
        };

        float m1,v1,s1, m2,v2,s2, m3,v3,s3, m4,v4,s4, m5,v5,s5;
        calc_stats(s_a1, sq_a1, m1, v1, s1);
        calc_stats(s_a2, sq_a2, m2, v2, s2);
        calc_stats(s_b1, sq_b1, m3, v3, s3);
        calc_stats(s_b2, sq_b2, m4, v4, s4);
        calc_stats(s_st, sq_st, m5, v5, s5);

        for(size_t j = i; j < end_idx; ++j) {
            rawRows[j].acc1_mean = m1; rawRows[j].acc1_var = v1; rawRows[j].acc1_std = s1;
            rawRows[j].acc2_mean = m2; rawRows[j].acc2_var = v2; rawRows[j].acc2_std = s2;
            rawRows[j].brk1_mean = m3; rawRows[j].brk1_var = v3; rawRows[j].brk1_std = s3;
            rawRows[j].brk2_mean = m4; rawRows[j].brk2_var = v4; rawRows[j].brk2_std = s4;
            rawRows[j].steer_mean = m5; rawRows[j].steer_var = v5; rawRows[j].steer_std = s5;
        }
    }

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_loadedData = std::move(rawRows);
    m_isFinished = true;
    m_isLoading = false;
}

// ULTRA-FAST Zero-Allocation CSV Parser. Avoids any std::string creation.
void FileService::loadCsv(const std::string& filePath) { 
    std::ifstream file(filePath, std::ios::binary | std::ios::ate); 
    if (!file.is_open()) { 
        m_isLoading = false; m_isFinished = true; return; 
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(fileSize + 1);
    file.read(buffer.data(), fileSize);
    buffer[fileSize] = '\0'; // Ensure null-termination

    std::vector<NativeTelemetryRow> rawRows;
    rawRows.reserve(fileSize / 300); // Prevent reallocation to avoid crashes

    const char* p = buffer.data();
    const char* end = p + fileSize;

    // Skip the header line
    while (p < end && *p != '\n') p++;
    if (p < end) p++;

    std::vector<const char*> cols(110, nullptr);

    while (p < end) {
        if (m_cancelRequested.load()) break; 

        int colIdx = 0;
        cols[0] = p;
        
        // Inline tokenization: replace ';' with '\0' to use fast std::atof on pointers
        while (p < end && *p != '\n' && *p != '\r') {
            if (*p == ';') {
                *const_cast<char*>(p) = '\0'; 
                colIdx++;
                if (colIdx < 110) cols[colIdx] = p + 1;
            }
            p++;
        }
        
        // Fix: Ensure the very last token is also null-terminated! (Fixes lost packets issue)
        if (p < end) {
            *const_cast<char*>(p) = '\0';
            p++;
        }

        // Only parse if the row has the expected structure
        if (colIdx >= 105) {
            NativeTelemetryRow nat;
            
            nat.timestamp = d_atof(cols[0]);
            nat.front_left_velocity = f_atof(cols[1]);
            nat.front_right_velocity = f_atof(cols[2]);
            nat.rear_left_velocity = f_atof(cols[3]);
            nat.rear_right_velocity = f_atof(cols[4]);
            nat.front_left_suspension = f_atof(cols[5]);
            nat.front_right_suspension = f_atof(cols[6]);
            nat.accelerator1 = f_atof(cols[7]);
            nat.accelerator2 = f_atof(cols[8]);
            nat.accelerator_mapped = f_atof(cols[9]);
            nat.brake1 = f_atof(cols[10]);
            nat.brake2 = f_atof(cols[11]);
            nat.steer = f_atof(cols[12]);
            
            nat.sdc = i_atoi(cols[13]);
            nat.ready_to_drive_button = i_atoi(cols[14]);
            nat.ecu_reset_button = i_atoi(cols[15]);
            nat.tractive_system_on_button = i_atoi(cols[16]);
            
            nat.emma_current = f_atof(cols[17]);
            nat.emma_voltage = f_atof(cols[18]);
            nat.emma_yaw = f_atof(cols[19]);
            
            // Fix: Parse the string mask back into bits properly (Live CSV uses Text)
            auto parseEmmaError = [](const char* s) -> uint16_t {
                if (!s) return 0;
                if (std::strstr(s, "EMMA_STATUS_OK")) return 0;
                uint16_t err = 0;
                if (std::strstr(s, "ERR_UNDERVOLTAGE")) err |= 0x0001;
                if (std::strstr(s, "ERR_OVERVOLTAGE")) err |= 0x0002;
                if (std::strstr(s, "ERR_OVERTEMP_CELL")) err |= 0x0004;
                if (std::strstr(s, "ERR_UNDERTEMP_CELL")) err |= 0x0008;
                if (std::strstr(s, "ERR_OVERTEMP_PCB")) err |= 0x0010;
                if (std::strstr(s, "ERR_UNDERTEMP_PCB")) err |= 0x0020;
                if (std::strstr(s, "ERR_CELL_MISMATCH")) err |= 0x0040;
                if (std::strstr(s, "ERR_BSPD")) err |= 0x0080;
                if (std::strstr(s, "ERR_DAISY_CHAIN")) err |= 0x0100;
                if (std::strstr(s, "IMD_AMS_LATCH")) err |= 0x0200;
                if (std::strstr(s, "EMMA_FINISH_PRECHARGE")) err |= 0x0800;
                if (std::strstr(s, "ERR_CRITICAL_FAULT")) err |= 0x8000;
                return err;
            };
            nat.emma_error = parseEmmaError(cols[20]);

            nat.mean_velocity = f_atof(cols[21]);
            nat.real_yaw_rate = f_atof(cols[22]);
            nat.total_torque_request = f_atof(cols[23]);
            nat.torque_tv_L = f_atof(cols[24]);
            nat.torque_tv_R = f_atof(cols[25]);
            nat.slip_L = f_atof(cols[26]);
            nat.slip_R = f_atof(cols[27]);
            nat.torque_reduction_L = f_atof(cols[28]);
            nat.torque_reduction_R = f_atof(cols[29]);
            nat.final_torque_target_L = f_atof(cols[30]);
            nat.final_torque_target_R = f_atof(cols[31]);
            
            nat.inv_L_torqueCurrent = f_atof(cols[32]);
            nat.inv_L_magnetizingCurrent = f_atof(cols[33]);
            nat.inv_L_tempMotor = f_atof(cols[34]);
            nat.inv_R_torqueCurrent = f_atof(cols[35]);
            nat.inv_R_magnetizingCurrent = f_atof(cols[36]);
            nat.inv_R_tempMotor = f_atof(cols[37]);
            
            nat.left_coolant_temp = f_atof(cols[38]);
            nat.right_coolant_temp = f_atof(cols[39]);

            auto parseFsm = [](const char* s) -> uint8_t {
                if (!s) return 0;
                if (std::strstr(s, "SYSTEM_READY")) return 1;
                if (std::strstr(s, "DC_CAPACITOR_CHARGE")) return 2;
                if (std::strstr(s, "DC_OK")) return 3;
                if (std::strstr(s, "ENABLE_INVERTER")) return 4;
                if (std::strstr(s, "ENABLED")) return 5;
                if (std::strstr(s, "ON")) return 6;
                if (std::strstr(s, "OK")) return 7;
                if (std::strstr(s, "ERROR")) return 8;
                return 0; // OFF
            };

            auto parseTsFsm = [](const char* s) -> uint8_t {
                if (!s) return 0;
                if (std::strstr(s, "START_LIGHT_TEST")) return 1;
                if (std::strstr(s, "WAIT_LIGHT_TEST")) return 2;
                if (std::strstr(s, "POWER_OFF")) return 3;
                if (std::strstr(s, "TS_WAIT_ACTIVATION")) return 4;
                if (std::strstr(s, "TS_WAIT_PRECHARGE")) return 5;
                if (std::strstr(s, "TS_ACTIVE")) return 6;
                if (std::strstr(s, "R2D_WAIT")) return 7;
                if (std::strstr(s, "R2D_ACTIVE")) return 8;
                if (std::strstr(s, "ERROR_STATE")) return 9;
                return 0; // ENTRY_STATE
            };

            nat.left_inverter_fsm = parseFsm(cols[40]);
            nat.right_inverter_fsm = parseFsm(cols[41]);
            nat.tractive_system_fsm = parseTsFsm(cols[42]);
            
            nat.ECU_Mode = 0;
            if (cols[43]) {
                if (std::strstr(cols[43], "ENDURANCE")) nat.ECU_Mode = 1;
                else if (std::strstr(cols[43], "ACCELERATION")) nat.ECU_Mode = 2;
                else if (std::strstr(cols[43], "TEST")) nat.ECU_Mode = 4;
            }

            nat.state_of_charge = i_atoi(cols[44]);

            for(int i=0; i<14; ++i) nat.voltages[i] = f_atof(cols[45+i]);
            for(int i=0; i<14; ++i) {
                nat.temperatures1[i] = f_atof(cols[59 + (i*2)]);
                nat.temperatures2[i] = f_atof(cols[60 + (i*2)]);
            }

            nat.online_modules = static_cast<uint16_t>(i_atoi(cols[87]));

            nat.acc1_mean = f_atof(cols[88]); nat.acc1_var = f_atof(cols[89]); nat.acc1_std = f_atof(cols[90]);
            nat.acc2_mean = f_atof(cols[91]); nat.acc2_var = f_atof(cols[92]); nat.acc2_std = f_atof(cols[93]);
            nat.brk1_mean = f_atof(cols[94]); nat.brk1_var = f_atof(cols[95]); nat.brk1_std = f_atof(cols[96]);
            nat.brk2_mean = f_atof(cols[97]); nat.brk2_var = f_atof(cols[98]); nat.brk2_std = f_atof(cols[99]);
            nat.steer_mean = f_atof(cols[100]); nat.steer_var = f_atof(cols[101]); nat.steer_std = f_atof(cols[102]);

            nat.lostA = i_atoi(cols[103]);
            nat.lostB = i_atoi(cols[104]);
            nat.lostTot = i_atoi(cols[105]);

            rawRows.push_back(nat);
        }

        // Advance to next line correctly
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_loadedData = std::move(rawRows);
    m_isFinished = true;
    m_isLoading = false;
}