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
    uint32_t timestamp;

    // --- Wheel velocities ---
    float front_left_velocity;
    float front_right_velocity;
    float rear_left_velocity;
    float rear_right_velocity;

    // --- Suspension length ---
    float front_left_suspension;
    float front_right_suspension;

    // --- APPS ---
    uint16_t accelerator1;
    uint16_t accelerator2;
    float accelerator_mapped;

    // --- Brake ---
    uint16_t brake1;
    uint16_t brake2;

    // --- Steer ---
    float steer;

    // --- Digital in ---
    uint8_t sdc;
    uint8_t ready_to_drive_button;
    uint8_t ecu_reset_button;
    uint8_t tractive_system_on_button;

    // --- Battery (EMMA) ---
    float emma_current;
    float emma_voltage;
    float emma_yaw;
    uint16_t emma_error;

    // --- Vehicle Dynamics & Powertrain Targets ---
    float mean_velocity;
    float real_yaw_rate;
    float total_torque_request;
    float torque_tv_L;
    float torque_tv_R;
    float slip_L;
    float slip_R;
    float torque_reduction_L;
    float torque_reduction_R;
    float final_torque_target_L;
    float final_torque_target_R;

    // --- Inverter Internal Data ---
    int16_t inv_L_torqueCurrent;
    int16_t inv_L_magnetizingCurrent;
    int16_t inv_L_tempMotor;
    int16_t inv_R_torqueCurrent;
    int16_t inv_R_magnetizingCurrent;
    int16_t inv_R_tempMotor;

    // --- External Thermal Data ---
    float left_motor_temp;
    float right_motor_temp;
    float left_coolant_temp;
    float right_coolant_temp;

    // --- State machines ---
    uint8_t left_inverter_fsm;
    uint8_t right_inverter_fsm;
    uint8_t tractive_system_fsm;

    // --- ECU mode ---
    uint8_t ECU_Mode;

    // --- CRC for corrupted packages ---
    uint16_t crc16;
} TelemetryPacket_t;

typedef struct {
    // --- Telemetry packet id and metadata ---
    uint8_t startByte1; // 0xEE
    uint8_t startByte2; // 0x11
    uint32_t timestamp;

    // --- Inverter states ---
    uint8_t state_of_charge;
    uint8_t module_voltage[14];
    uint16_t module_temperatures[28];

    // --- CRC for corrupted packages ---
    uint16_t crc16;
} TelemetryPacket_EMMA_t;
#pragma pack(pop)

using DbRow = std::map<std::string, std::string>;

uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length);

class PacketParser {
public:
    PacketParser();

    // LIVE: Estrae i pacchetti dai byte in ingresso. Ritorna true e compila l'outRow se trova un pacchetto da 5ms.
    bool parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow);
    
    // FILE: Converte la riga del CSV in una mappa
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

    // Memoria per incollare le celle del pacchetto 200ms nel pacchetto 5ms
    DbRow m_lastPacketBData; 
};