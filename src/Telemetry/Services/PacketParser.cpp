#include <sstream>
#include <unordered_set>
#include <iostream> 
#include "Telemetry/data_acquisition/PacketParser.hpp"

// Tipi di dato
namespace {
    const std::unordered_set<std::string> integerSensorLabels = {
        "ACC1A", "ACC2A"
    };
    const std::unordered_set<std::string> doubleSensorLabels = {
        "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };
    const std::unordered_set<std::string> statusLedLabels = {
        "SDC_INPUT", "RESET_BUTTON", "TS_ON_BUTTON", "R2D_BUTTON"
    };
    const std::unordered_set<std::string> inverterFsmLabels = {
        "LEFT_INVERTER_FSM", "RIGHT_INVERTER_FSM", "INV1", "INV2"
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
        packet.packetType = PacketType::INVERTER; 
        packet.label = part2;
        packet.data = part3;
        return packet; 
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