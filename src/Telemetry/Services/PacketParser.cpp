#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <sstream>
#include <iomanip>

PacketParser::PacketParser() {
    resetCounters();
}

uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void PacketParser::resetCounters() {
    m_lastTimestampA = 0; m_lastTimestampB = 0;
    m_lostA = 0; m_lostB = 0;
    m_firstA = true; m_firstB = true;
    m_lastPacketBData.clear();
}

static std::string formatDouble(double val, int dec) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(dec) << val;
    return ss.str();
}

void PacketParser::populateRowFromA(const TelemetryPacket_t* dataA, const DbRow& cachedDataB, DbRow& row) {
    double time_sec = dataA->timestamp * 0.005;
    row["timestamp"] = formatDouble(time_sec, 3);
    
    // --- 1. DATI PACCHETTO A (5ms) TUTTI PRESENTI ---
    row["VELASX"] = formatDouble(dataA->front_left_velocity, 1);
    row["VELADX"] = formatDouble(dataA->front_right_velocity, 1);
    row["VELPSX"] = formatDouble(dataA->rear_left_velocity, 1);
    row["VELPDX"] = formatDouble(dataA->rear_right_velocity, 1);
    
    row["SOSPASX"] = formatDouble(dataA->front_left_suspension, 2);
    row["SOSPADX"] = formatDouble(dataA->front_right_suspension, 2);
    
    row["ACC1"] = std::to_string(dataA->accelerator1);
    row["ACC2"] = std::to_string(dataA->accelerator2);
    row["ACC_MAPPED"] = formatDouble(dataA->accelerator_mapped, 1);
    
    row["BRK1"] = std::to_string(dataA->brake1);
    row["BRK2"] = std::to_string(dataA->brake2);
    row["STEER"] = formatDouble(dataA->steer, 1);
    
    row["SDC_INPUT"] = std::to_string(dataA->sdc);
    row["R2D_BUTTON"] = std::to_string(dataA->ready_to_drive_button);
    row["RESET_BUTTON"] = std::to_string(dataA->ecu_reset_button);
    row["TS_ON_BUTTON"] = std::to_string(dataA->tractive_system_on_button);
    
    row["EMMA_CURRENT"] = formatDouble(dataA->emma_current, 1);
    row["EMMA_VOLTAGE"] = formatDouble(dataA->emma_voltage, 1);
    row["EMMA_YAW"] = formatDouble(dataA->emma_yaw, 2);
    row["EMMA_ERROR"] = std::to_string(dataA->emma_error);
    
    row["MEAN_VELOCITY"] = formatDouble(dataA->mean_velocity, 1);
    row["REAL_YAW"] = formatDouble(dataA->real_yaw_rate, 2);
    row["TOTAL_TORQUE"] = formatDouble(dataA->total_torque_request, 1);
    row["TORQUE_TV_L"] = formatDouble(dataA->torque_tv_L, 1);
    row["TORQUE_TV_R"] = formatDouble(dataA->torque_tv_R, 1);
    row["SLIP_L"] = formatDouble(dataA->slip_L, 2);
    row["SLIP_R"] = formatDouble(dataA->slip_R, 2);
    row["TORQUE_RED_L"] = formatDouble(dataA->torque_reduction_L, 1);
    row["TORQUE_RED_R"] = formatDouble(dataA->torque_reduction_R, 1);
    row["FINAL_TORQUE_L"] = formatDouble(dataA->final_torque_target_L, 1);
    row["FINAL_TORQUE_R"] = formatDouble(dataA->final_torque_target_R, 1);

    row["INV_L_TRQ_CURR"] = std::to_string(dataA->inv_L_torqueCurrent);
    row["INV_L_MAG_CURR"] = std::to_string(dataA->inv_L_magnetizingCurrent);
    row["INV_L_TEMP_MOT"] = std::to_string(dataA->inv_L_tempMotor);
    row["INV_R_TRQ_CURR"] = std::to_string(dataA->inv_R_torqueCurrent);
    row["INV_R_MAG_CURR"] = std::to_string(dataA->inv_R_magnetizingCurrent);
    row["INV_R_TEMP_MOT"] = std::to_string(dataA->inv_R_tempMotor);
    
    row["TMPDX"] = formatDouble(dataA->right_coolant_temp, 1);
    row["TMPSX"] = formatDouble(dataA->left_coolant_temp, 1);
    row["TMPMOTORDX"] = formatDouble(dataA->right_motor_temp, 1);
    row["TMPMOTORSX"] = formatDouble(dataA->left_motor_temp, 1);
    
    row["LEFT_INVERTER_FSM"] = std::to_string(dataA->left_inverter_fsm);
    row["RIGHT_INVERTER_FSM"] = std::to_string(dataA->right_inverter_fsm);
    row["TRACTIVE_SYS_FSM"] = std::to_string(dataA->tractive_system_fsm);
    row["ECU_MODE"] = std::to_string(dataA->ECU_Mode);

    // --- 2. DATI PACCHETTO B (200ms) INCOLLATI DALLA CACHE ---
    for (const auto& kv : cachedDataB) {
        row[kv.first] = kv.second;
    }

    // --- 3. PACCHETTI PERSI ---
    row["LOST_PACKETS_A"] = std::to_string(m_lostA);
    row["LOST_PACKETS_B"] = std::to_string(m_lostB);
    row["LOST_PACKETS_TOT"] = std::to_string(getTotalLost());
}

void PacketParser::populateRowFromB(const TelemetryPacket_EMMA_t* data) {
    m_lastPacketBData["STATE_OF_CHARGE"] = std::to_string(data->state_of_charge);
    // Srotoliamo i Volt in 14 colonne
    for(int i=0; i<14; ++i) {
        m_lastPacketBData["TENSM" + std::to_string(i+1)] = std::to_string(data->module_voltage[i]);
    }
    // Srotoliamo le Temp in 28 colonne
    for(int i=0; i<28; ++i) {
        int mod = (i / 2) + 1;
        int tIdx = (i % 2) + 1;
        m_lastPacketBData["TMP" + std::to_string(tIdx) + "M" + std::to_string(mod)] = formatDouble(data->module_temperatures[i] / 10.0, 1); 
    }
}

bool PacketParser::parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow) {
    while (rxBuffer.size() >= 4) {
        size_t syncIndex = std::string::npos;
        int packetType = 0; 
        
        // Controllo Header modificato: 0xCC 0x11 per Type 1, 0xEE 0x11 per Type 2
        for (size_t i = 0; i < rxBuffer.size() - 1; ++i) {
            if (rxBuffer[i] == 0xCC && rxBuffer[i+1] == 0x11 && rxBuffer.size() - i >= sizeof(TelemetryPacket_t)) {
                syncIndex = i; packetType = 1; break;
            } else if (rxBuffer[i] == 0xEE && rxBuffer[i+1] == 0x11 && rxBuffer.size() - i >= sizeof(TelemetryPacket_EMMA_t)) {
                syncIndex = i; packetType = 2; break;
            }
        }

        if (syncIndex == std::string::npos) return false;
        
        // Pulizia spazzatura precedente al pacchetto
        if (syncIndex > 0) rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + syncIndex);

        if (packetType == 1) { 
            TelemetryPacket_t* data = reinterpret_cast<TelemetryPacket_t*>(rxBuffer.data());
            uint16_t crc = Calculate_CRC16(rxBuffer.data(), sizeof(TelemetryPacket_t) - 2);
            
            if (crc == data->crc16) {
                // MODIFICA QUI: Aggiunto filtro limite diff < 5000
                if (!m_firstA && data->timestamp > m_lastTimestampA) {
                    uint32_t diff = data->timestamp - m_lastTimestampA;
                    if (diff > 1 && diff < 5000) { 
                        m_lostA += (diff - 1);
                    }
                }
                m_lastTimestampA = data->timestamp; m_firstA = false;

                outRow.clear();
                populateRowFromA(data, m_lastPacketBData, outRow);
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + sizeof(TelemetryPacket_t));
                return true; 
            } else {
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + 1); 
            }
            
        } else if (packetType == 2) { 
            TelemetryPacket_EMMA_t* data = reinterpret_cast<TelemetryPacket_EMMA_t*>(rxBuffer.data());
            uint16_t crc = Calculate_CRC16(rxBuffer.data(), sizeof(TelemetryPacket_EMMA_t) - 2);
            
            if (crc == data->crc16) {
                // MODIFICA QUI: Aggiunto filtro limite
                if (!m_firstB && data->timestamp > m_lastTimestampB) {
                    uint32_t diff = data->timestamp - m_lastTimestampB;
                    if (diff > 40 && diff < 100000) {
                        m_lostB += (diff / 40) - 1;
                    }
                }
                m_lastTimestampB = data->timestamp; m_firstB = false;

                populateRowFromB(data); 
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + sizeof(TelemetryPacket_EMMA_t));
            } else {
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + 1);
            }
        }
    }
    return false;
}

bool PacketParser::parseCsvLine(const std::string& line, DbRow& outRow) {
    if (line.empty()) return false;
    std::stringstream ss(line);
    std::string token;
    auto columns = getColumnOrder();
    int idx = 0;
    while(std::getline(ss, token, ';') && idx < columns.size()) {
        outRow[columns[idx]] = token;
        idx++;
    }
    return idx > 1;
}

std::vector<std::string> PacketParser::getColumnOrder() {
    std::vector<std::string> cols = {
        "timestamp", 
        "VELASX", "VELADX", "VELPSX", "VELPDX", 
        "SOSPASX", "SOSPADX",
        "ACC1", "ACC2", "ACC_MAPPED", 
        "BRK1", "BRK2", 
        "STEER",
        "SDC_INPUT", "R2D_BUTTON", "RESET_BUTTON", "TS_ON_BUTTON",
        "EMMA_CURRENT", "EMMA_VOLTAGE", "EMMA_YAW", "EMMA_ERROR",
        "MEAN_VELOCITY", "REAL_YAW", "TOTAL_TORQUE", 
        "TORQUE_TV_L", "TORQUE_TV_R", "SLIP_L", "SLIP_R", 
        "TORQUE_RED_L", "TORQUE_RED_R", "FINAL_TORQUE_L", "FINAL_TORQUE_R",
        "INV_L_TRQ_CURR", "INV_L_MAG_CURR", "INV_L_TEMP_MOT",
        "INV_R_TRQ_CURR", "INV_R_MAG_CURR", "INV_R_TEMP_MOT",
        "TMPDX", "TMPSX", "TMPMOTORDX", "TMPMOTORSX",
        "LEFT_INVERTER_FSM", "RIGHT_INVERTER_FSM", "TRACTIVE_SYS_FSM", 
        "ECU_MODE",
        "STATE_OF_CHARGE", 
        "LOST_PACKETS_A", "LOST_PACKETS_B", "LOST_PACKETS_TOT"
    };
    
    // Le 14 tensioni
    for(int i=1; i<=14; ++i) cols.push_back("TENSM" + std::to_string(i));
    
    // Le 28 temperature
    for(int i=1; i<=14; ++i) {
        cols.push_back("TMP1M" + std::to_string(i));
        cols.push_back("TMP2M" + std::to_string(i));
    }
    
    // Campi statistici aggiunti dinamicamente dalla varianza (così vengono mostrati nel CSV/Dataset)
    const std::vector<std::string> targetSensors = {"ACC1", "ACC2", "BRK1", "BRK2", "STEER"};
    for (const auto& sensor : targetSensors) {
        cols.push_back(sensor + "_MEAN"); 
        cols.push_back(sensor + "_VAR"); 
        cols.push_back(sensor + "_STD");
    }
    return cols;
}