#include <sstream>
#include <iomanip>
#include <iostream> 
#include "Telemetry/data_acquisition/PacketParser.hpp"

// Mapping functions to convert raw values into strings for specific fields 
static std::string getEmmaErrorStr(uint16_t error) {
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

static std::string getInverterFsmStr(uint8_t val) {
    const char* states[] = {
        "OFF", "SYSTEM_READY", "DC_CAPACITOR_CHARGE", "DC_OK", 
        "ENABLE_INVERTER", "ENABLED", "ON", "OK", "ERROR"
    };
    if (val <= 8) return states[val];
    return "UNKNOWN";
}

static std::string getTractiveFsmStr(uint8_t val) {
    const char* states[] = {
        "ENTRY_STATE", "START_LIGHT_TEST", "WAIT_LIGHT_TEST", "POWER_OFF", 
        "TS_WAIT_ACTIVATION", "TS_WAIT_PRECHARGE", "TS_ACTIVE", "R2D_WAIT", 
        "R2D_ACTIVE", "ERROR_STATE"
    };
    if (val <= 9) return states[val];
    return "UNKNOWN";
}

static std::string getEcuModeStr(uint8_t val) {
    if (val & 0x01) return "ENDURANCE";
    if (val & 0x02) return "ACCELERATION";
    if (val & 0x04) return "TEST";
    return "UNKNOWN";
}

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
    
    // Data from packet 0xCC (5ms) with formatting and string conversion as needed
    row["front_left_velocity"] = formatDouble(dataA->front_left_velocity, 1);
    row["front_right_velocity"] = formatDouble(dataA->front_right_velocity, 1);
    row["rear_left_velocity"] = formatDouble(dataA->rear_left_velocity, 1);
    row["rear_right_velocity"] = formatDouble(dataA->rear_right_velocity, 1);

    row["front_left_suspension"] = formatDouble(dataA->front_left_suspension, 1);
    row["front_right_suspension"] = formatDouble(dataA->front_right_suspension, 1);

    row["accelerator1"] = std::to_string(dataA->accelerator1);
    row["accelerator2"] = std::to_string(dataA->accelerator2);
    row["accelerator_mapped"] = formatDouble(dataA->accelerator_mapped, 1);
    
    row["brake1"] = std::to_string(dataA->brake1);
    row["brake2"] = std::to_string(dataA->brake2);
    row["steer"] = formatDouble(dataA->steer, 1);
    
    row["sdc"] = std::to_string(dataA->sdc);
    row["ready_to_drive_button"] = std::to_string(dataA->ready_to_drive_button);
    row["ecu_reset_button"] = std::to_string(dataA->ecu_reset_button);
    row["tractive_system_on_button"] = std::to_string(dataA->tractive_system_on_button);
    
    row["emma_current"] = formatDouble(dataA->emma_current, 1);
    row["emma_voltage"] = formatDouble(dataA->emma_voltage, 1);
    row["emma_yaw"] = formatDouble(dataA->emma_yaw, 2);
    row["emma_error"] = getEmmaErrorStr(dataA->emma_error);
    
    row["mean_velocity"] = formatDouble(dataA->mean_velocity, 1);
    row["real_yaw_rate"] = formatDouble(dataA->real_yaw_rate, 2);
    row["total_torque_request"] = formatDouble(dataA->total_torque_request, 2);
    row["torque_tv_L"] = formatDouble(dataA->torque_tv_L, 2);
    row["torque_tv_R"] = formatDouble(dataA->torque_tv_R, 2);
    row["slip_L"] = formatDouble(dataA->slip_L, 3);
    row["slip_R"] = formatDouble(dataA->slip_R, 3);
    row["torque_reduction_L"] = formatDouble(dataA->torque_reduction_L, 2);
    row["torque_reduction_R"] = formatDouble(dataA->torque_reduction_R, 2);
    row["final_torque_target_L"] = formatDouble(dataA->final_torque_target_L, 2);
    row["final_torque_target_R"] = formatDouble(dataA->final_torque_target_R, 2);

    row["inv_L_torqueCurrent"] = formatDouble(dataA->inv_L_torqueCurrent / 10.0, 1);
    row["inv_L_magnetizingCurrent"] = formatDouble(dataA->inv_L_magnetizingCurrent / 10.0, 1);
    row["inv_L_tempMotor"] = formatDouble(dataA->inv_L_tempMotor / 10.0, 1);
    row["inv_R_torqueCurrent"] = formatDouble(dataA->inv_R_torqueCurrent / 10.0, 1);
    row["inv_R_magnetizingCurrent"] = formatDouble(dataA->inv_R_magnetizingCurrent / 10.0, 1);
    row["inv_R_tempMotor"] = formatDouble(dataA->inv_R_tempMotor / 10.0, 1);
    
    row["left_coolant_temp"] = formatDouble(dataA->left_coolant_temp, 1);
    row["right_coolant_temp"] = formatDouble(dataA->right_coolant_temp, 1);
    
    row["left_inverter_fsm"] = getInverterFsmStr(dataA->left_inverter_fsm);
    row["right_inverter_fsm"] = getInverterFsmStr(dataA->right_inverter_fsm);
    row["tractive_system_fsm"] = getTractiveFsmStr(dataA->tractive_system_fsm);
    row["ECU_Mode"] = getEcuModeStr(dataA->ECU_Mode);

    // Data from packet 0xEE (200ms) 
    for (const auto& kv : cachedDataB) {
        row[kv.first] = kv.second;
    }

    // Lost packet calculation
    row["lost_packets_A"] = std::to_string(m_lostA);
    row["lost_packets_B"] = std::to_string(m_lostB);
    row["lost_packets_tot"] = std::to_string(getTotalLost());
}

void PacketParser::populateRowFromB(const TelemetryPacket_EMMA_t* data) {
    m_lastPacketBData["state_of_charge"] = std::to_string(data->state_of_charge);
    // Looking at the Voltages in 14 columns
    for(int i=0; i<14; ++i) {
        float v_real = (static_cast<float>(data->module_voltage[i]) + 200.0f) / 100.0f;
        m_lastPacketBData["VoltageModule" + std::to_string(i+1)] = formatDouble(v_real, 2);
    }
    // Looking at the Temperatures in 28 columns
    for(int i=0; i<28; ++i) {
        int mod = (i / 2) + 1;
        int tIdx = (i % 2) + 1;
        m_lastPacketBData["Temperature" + std::to_string(tIdx) + "Module" + std::to_string(mod)] = formatDouble(data->module_temperatures[i] / 10.0, 1); 
    }
}

bool PacketParser::parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow) {
    while (rxBuffer.size() >= 4) {
        size_t syncIndex = std::string::npos;
        int packetType = 0; 
        
        for (size_t i = 0; i < rxBuffer.size() - 1; ++i) {
            if (rxBuffer[i] == 0xCC && rxBuffer[i+1] == 0x11 && rxBuffer.size() - i >= sizeof(TelemetryPacket_t)) {
                syncIndex = i; packetType = 1; break;
            } else if (rxBuffer[i] == 0xEE && rxBuffer[i+1] == 0x11 && rxBuffer.size() - i >= sizeof(TelemetryPacket_EMMA_t)) {
                syncIndex = i; packetType = 2; break;
            }
        }

        if (syncIndex == std::string::npos) return false;
        
        // Trash bytes before the valid packet are discarded with a warning
        if (syncIndex > 0) {
            // Extract the timestamp of the valid packet (current) to which we are synchronizing
            uint32_t currentTs = 0;
            std::string pktName = (packetType == 1) ? "0xCC" : "0xEE";
            
            if (packetType == 1) {
                TelemetryPacket_t* tempA = reinterpret_cast<TelemetryPacket_t*>(rxBuffer.data() + syncIndex);
                currentTs = tempA->timestamp;
            } else if (packetType == 2) {
                TelemetryPacket_EMMA_t* tempB = reinterpret_cast<TelemetryPacket_EMMA_t*>(rxBuffer.data() + syncIndex);
                currentTs = tempB->timestamp;
            }

            std::cerr << "\n[WARNING] Finded " << syncIndex 
                      << " bytes of garbage before the current packet [" << pktName << " | Timestamp: " << currentTs << "].\n"
                      << "Discarded data: ";
            for (size_t k = 0; k < syncIndex; ++k) {
                std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)rxBuffer[k] << " ";
            }
            std::cerr << std::dec << std::endl; 
            
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + syncIndex);
        }

        if (packetType == 1) { 
            TelemetryPacket_t* dataA = reinterpret_cast<TelemetryPacket_t*>(rxBuffer.data());
            uint16_t crcA = Calculate_CRC16(rxBuffer.data(), sizeof(TelemetryPacket_t) - 2);
            
            if (crcA == dataA->crc16) {
                
                // Lookahead to check if the next packet is a valid 0xEE and if so, parse it immediately to have the freshest data for the combined row
                size_t packetB_start = sizeof(TelemetryPacket_t);
                
                if (rxBuffer.size() >= packetB_start + 2) {
                    if (rxBuffer[packetB_start] == 0xEE && rxBuffer[packetB_start+1] == 0x11) {
                        
                        if (rxBuffer.size() < packetB_start + sizeof(TelemetryPacket_EMMA_t)) {
                            return false; 
                        }
                        
                        TelemetryPacket_EMMA_t* dataB = reinterpret_cast<TelemetryPacket_EMMA_t*>(rxBuffer.data() + packetB_start);
                        uint16_t crcB = Calculate_CRC16(rxBuffer.data() + packetB_start, sizeof(TelemetryPacket_EMMA_t) - 2);
                        
                        if (crcB == dataB->crc16) {
                            if (!m_firstB && dataB->timestamp > m_lastTimestampB) {
                                uint32_t diff = dataB->timestamp - m_lastTimestampB;
                                if (diff > 40 && diff < 100000) {
                                    m_lostB += (diff / 40) - 1;
                                }
                            }
                            m_lastTimestampB = dataB->timestamp; 
                            m_firstB = false;
                            
                            populateRowFromB(dataB); 
                            
                            if (!m_firstA && dataA->timestamp > m_lastTimestampA) {
                                uint32_t diff = dataA->timestamp - m_lastTimestampA;
                                if (diff > 1 && diff < 5000) { 
                                    m_lostA += (diff - 1);
                                }
                            }
                            m_lastTimestampA = dataA->timestamp; 
                            m_firstA = false;

                            outRow.clear();
                            populateRowFromA(dataA, m_lastPacketBData, outRow);
                            
                            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + packetB_start + sizeof(TelemetryPacket_EMMA_t));
                            return true;
                        }
                    }
                } else if (rxBuffer.size() >= packetB_start && rxBuffer.size() < packetB_start + 2) {
                    return false;
                }

                if (!m_firstA && dataA->timestamp > m_lastTimestampA) {
                    uint32_t diff = dataA->timestamp - m_lastTimestampA;
                    if (diff > 1 && diff < 5000) { 
                        m_lostA += (diff - 1);
                    }
                }
                m_lastTimestampA = dataA->timestamp; m_firstA = false;

                outRow.clear();
                populateRowFromA(dataA, m_lastPacketBData, outRow);
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + sizeof(TelemetryPacket_t));
                return true; 
                
            } else {
                std::cerr << "\n[ERROR] Packet 0xCC corrupted (timestamp: " << dataA->timestamp << ") CRC expected: 0x" 
                          << std::hex << dataA->crc16 << ", CRC calculated: 0x" << crcA << std::dec << ".\n" << std::endl;         
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + 1); 
            }
            
        } else if (packetType == 2) { 
            TelemetryPacket_EMMA_t* data = reinterpret_cast<TelemetryPacket_EMMA_t*>(rxBuffer.data());
            uint16_t crc = Calculate_CRC16(rxBuffer.data(), sizeof(TelemetryPacket_EMMA_t) - 2);
            
            if (crc == data->crc16) {
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
                std::cerr << "\n[ERROR] Packet 0xEE corrupted (timestamp: " << data->timestamp << ") CRC expected: 0x" 
                          << std::hex << data->crc16 << ", CRC calculated: 0x" << crc << std::dec << ".\n" << std::endl;
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

// Dateset column order for CSV output and internal processing 
std::vector<std::string> PacketParser::getColumnOrder() {
    std::vector<std::string> cols = {
        "timestamp", 
        "front_left_velocity", "front_right_velocity", "rear_left_velocity", "rear_right_velocity",
        "front_left_suspension", "front_right_suspension",
        "accelerator1", "accelerator2", "accelerator_mapped",
        "brake1", "brake2",
        "steer",
        "sdc", "ready_to_drive_button", "ecu_reset_button", "tractive_system_on_button",
        "emma_current", "emma_voltage", "emma_yaw", "emma_error",
        "mean_velocity", "real_yaw_rate", "total_torque_request", 
        "torque_tv_L", "torque_tv_R", "slip_L", "slip_R", 
        "torque_reduction_L", "torque_reduction_R", "final_torque_target_L", "final_torque_target_R",
        "inv_L_torqueCurrent", "inv_L_magnetizingCurrent", "inv_L_tempMotor",
        "inv_R_torqueCurrent", "inv_R_magnetizingCurrent", "inv_R_tempMotor",
        "left_coolant_temp", "right_coolant_temp",
        "left_inverter_fsm", "right_inverter_fsm", "tractive_system_fsm", 
        "ECU_Mode",
        "state_of_charge", 
    };
    
    for(int i=1; i<=14; ++i) cols.push_back("VoltageModule" + std::to_string(i));
    
    for(int i=1; i<=14; ++i) {
        cols.push_back("Temperature1Module" + std::to_string(i));
        cols.push_back("Temperature2Module" + std::to_string(i));
    }
    
    // Statistics fields added dynamically from variance (so they are shown in CSV/Dataset)
    const std::vector<std::string> targetSensors = {"accelerator1", "accelerator2", "brake1", "brake2", "steer"};
    for (const auto& sensor : targetSensors) {
        cols.push_back(sensor + "_mean"); 
        cols.push_back(sensor + "_var"); 
        cols.push_back(sensor + "_std");
    }

    cols.push_back("lost_packets_A");
    cols.push_back("lost_packets_B");   
    cols.push_back("lost_packets_tot");
    
    return cols;
}