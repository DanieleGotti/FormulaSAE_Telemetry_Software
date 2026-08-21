#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

#pragma pack(push, 1)
typedef struct {
    // --- Telemetry packet id and metadata ---
    uint8_t startByte1; // 0xCC
    uint8_t startByte2; // 0x11
    uint32_t timestamp; // counter int

    // --- Wheel velocities ---
    float front_left_velocity; // km/h
    float front_right_velocity; // km/h
    float rear_left_velocity; // km/h
    float rear_right_velocity; // km/h

    // --- Suspension length ---
    float front_left_suspension; // cm
    float front_right_suspension; // cm

    // --- APPS ---
    uint16_t accelerator1; // 0-4095
    uint16_t accelerator2; // 0-4095
    float accelerator_mapped; // 0.0-1.0

    // --- Brake ---
    uint16_t brake1; // 0-4095
    uint16_t brake2; // 0-4095

    // --- Steer ---
    float steer; // degrees, -180 to 180

    // --- Digital in ---
    uint8_t sdc; // digital, 0 or 1
    uint8_t ready_to_drive_button; // digital, 0 or 1
    uint8_t ecu_reset_button; // digital, 0 or 1
    uint8_t tractive_system_on_button; // digital, 0 or 1

    // --- Battery (EMMA) ---
    float emma_current; // A
    float emma_voltage; // V
    float emma_yaw; // degrees/s
    uint16_t emma_error; // mask

    // --- Vehicle Dynamics & Powertrain Targets ---
    float mean_velocity; // m/s
    float real_yaw_rate; // degrees/s
    float total_torque_request; // Nm
    float torque_tv_L; // Nm
    float torque_tv_R; // Nm
    float slip_L; // dimensionless
    float slip_R; // dimensionless
    float torque_reduction_L; // Nm
    float torque_reduction_R; // Nm
    float final_torque_target_L; // Nm
    float final_torque_target_R; // Nm

    // --- Inverter Internal Data ---
    int16_t inv_L_torqueCurrent; // A (/10)
    int16_t inv_L_magnetizingCurrent; // A (/10)
    int16_t inv_L_tempMotor; // °C (/10)
    int16_t inv_R_torqueCurrent; // A (/10)
    int16_t inv_R_magnetizingCurrent; // A (/10)  
    int16_t inv_R_tempMotor; // °C (/10)

    // --- External Thermal Data ---
    float left_coolant_temp; // °C 
    float right_coolant_temp; // °C

    // --- State machines ---
    uint8_t left_inverter_fsm; // 0-7
    uint8_t right_inverter_fsm; // 0-7
    uint8_t tractive_system_fsm; // 0-9

    // --- ECU mode ---
    uint8_t ECU_Mode; // mask
    uint16_t IMDInsulationValue; //kOhm

    // --- CRC for corrupted packages ---
    uint16_t crc16; // int calculated
} TelemetryPacket_t;

typedef struct {
    // --- Telemetry packet id and metadata ---
    uint8_t startByte1; // 0xEE
    uint8_t startByte2; // 0x11
    uint32_t timestamp; // counter int

    // --- Inverter states ---
    uint8_t state_of_charge; // 0-100
    uint8_t module_voltage[14]; // V ((V+200)/100)
    uint16_t module_temperatures[28]; // °C (/10)
    uint16_t online_modules; // mask for online modules (1=online, 0=offline)

    // --- CRC for corrupted packages ---
    uint16_t crc16; // int calculated
} TelemetryPacket_EMMA_t;
#pragma pack(pop)

using DbRow = std::map<std::string, std::string>;

constexpr float HALL_MS_TO_KMH = 3.6f;
constexpr float HALL_ECU_SCALE_FIX = 1.0f;

uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length);

class PacketParser {
public:
    PacketParser();

    // LIVE: extract packets from incoming bytes. Returns true and populates outRow if a 5ms packet is found
    bool parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow);
    
    // FILE: Converts a CSV line into a map
    bool parseCsvLine(const std::string& line, DbRow& outRow);

    void resetCounters();

    uint32_t getLostA() const { return m_lostA; }
    uint32_t getLostB() const { return m_lostB; }
    uint32_t getTotalLost() const { return m_lostA + m_lostB; }

    static std::vector<std::string> getColumnOrder();

private:
    void populateRowFromA(const TelemetryPacket_t* dataA, const DbRow& cachedDataB, DbRow& row);
    void populateRowFromB(const TelemetryPacket_EMMA_t* data);

    uint32_t m_lastTimestampA = 0;
    uint32_t m_lastTimestampB = 0;
    uint32_t m_lostA = 0;
    uint32_t m_lostB = 0;
    bool m_firstA = true;
    bool m_firstB = true;

    // Memory for pasting cells from the 200ms packet into the 5ms packet
    DbRow m_lastPacketBData; 
};