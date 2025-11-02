#include <sstream>
#include <unordered_set>
#include <iostream>
#include <map>
#include "Telemetry/data_acquisition/PacketParser.hpp"

// Tipi di dato che arrivano dalla centralina
namespace {
    const std::unordered_set<std::string> integerSensorLabels = {
        "ACC1A", "ACC2A",
    };
    const std::unordered_set<std::string> doubleSensorLabels = {
        "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER",
        "SOSPASX", "SOSPADX", "SOSPPSX", "SOSPPDX"
    };
    const std::unordered_set<std::string> statusLedLabels = {
        "SDC_INPUT", "RESET_BUTTON", "TS_ON_BUTTON", "R2D_BUTTON"
    };
    const std::unordered_set<std::string> inverterFsmLabels = {
        "LEFT_INVERTER_FSM", "RIGHT_INVERTER_FSM"
    };
    const std::map<int, std::string> inverterStateMapping = {
        {0, "OFF"}, {1, "SYSTEM_READY"}, {2, "DC_CAPACITOR_CHARGE"},
        {3, "DC_OK"}, {4, "ENABLE_INVERTER"}, {5, "ENABLED"},
        {6, "ON"}, {7, "OK"}, {8, "ERROR"}
    };
    const std::string inverterStateKeyword = "STATE";
}

PacketParser PacketParser::parse(const std::string& line,  const std::chrono::system_clock::time_point& reception_time) {
    PacketParser packet; 
    packet.timestamp = reception_time;

    // Divide la riga in parti separate da spazi
    std::stringstream ss(line);
    std::string part1, part2, part3;
    ss >> part1 >> part2 >> part3;

    // I messaggi inverter hanno 3 parti: "STATE <LABEL> <VALUE>"
    if (part1 == inverterStateKeyword && inverterFsmLabels.count(part2) && !part3.empty()) {
        try {
            // Converte la terza parte in intero e lo cerca nella mappa
            int stateValue = std::stoi(part3);
            auto it = inverterStateMapping.find(stateValue);
            if (it != inverterStateMapping.end()) {
                packet.packetType = PacketType::INVERTER; 
                packet.label = part2;
                packet.data = it->second; 
                return packet; 
            }
        } catch (const std::exception& e) {
            // Errore di conversione, verrà marcato sotto come UNKNOWN
        }
    }

    // Gli altri messaggi hanno 2 parti: "<LABEL> <VALUE>"
    if (!part2.empty() && part3.empty()) {
        packet.label = part1;

        try {
            if (integerSensorLabels.count(part1)) {
                packet.packetType = PacketType::SENSOR_DATA;
                packet.data = std::stoi(part2);
                return packet; 
            }
            if (doubleSensorLabels.count(part1)) {
                packet.packetType = PacketType::SENSOR_DATA;
                packet.data = std::stod(part2);
                return packet;
            }
            if (statusLedLabels.count(part1)) {
                packet.packetType = PacketType::STATUS_LED;
                packet.data = std::stoi(part2);
                return packet; 
            }
        }
        catch (const std::invalid_argument& e) {
            // Errore di conversione, verrà marcato sotto come UNKNOWN
        }
    }
    
    // Se nessuna regola ha funzionato o c'è stato un errore di conversione
    packet.packetType = PacketType::UNKNOWN;
    packet.label = "MALFORMED";
    packet.data = line;
    return packet;
}