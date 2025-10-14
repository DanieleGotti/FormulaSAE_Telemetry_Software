#include <sstream>
#include <unordered_set>
#include "Telemetry/data_acquisition/PacketParser.hpp"

namespace {
    // Tipi di dato supportati 
    const std::unordered_set<std::string> integerSensorLabels = {
        "ACC1A", "ACC2A"
    };

    const std::unordered_set<std::string> doubleSensorLabels = {
        "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER"
    };

    const std::unordered_set<std::string> statusLedLabels = {
        "SDC_INPUT", "RESET_BUTTON", "TS_ON_BUTTON", "R2D_BUTTON"
    };

    const std::string inverterStateKeyword = "STATE";

    const std::unordered_set<std::string> inverterFsmLabels = {
        "RIGHT_INVERTER_FSM", "LEFT_INVERTER_FSM"
    };
}

PacketParser PacketParser::parse(const std::string& line) {
    PacketParser packet; 
    packet.timestamp = std::chrono::system_clock::now();
    
    std::stringstream ss(line);
    std::string part1, part2, part3;
    // Divido in 3 parole al massimo (3 servono per inverter, 2 per il resto)
    ss >> part1 >> part2 >> part3;

    // Se è un messaggio inverter valido 3 parole
    if (part1 == inverterStateKeyword && inverterFsmLabels.count(part2) && !part3.empty()) {
        packet.packetType = PacketType::INVERTER;
        packet.label = part2; // "RIGHT_INVERTER_FSM" o "LEFT_INVERTER_FSM"
        packet.data = part3; // "READY"
        return packet;
    }

    // Se non è un messaggio di stato 2 parole
    if (!part1.empty() && !part2.empty() && part3.empty()) {
        packet.label = part1;

        // Controlla se l'etichetta è in uno dei nostri set
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
            // Errore di conversione 
        }
    }
    
    // Se nessuna regola ha funzionato, salviamo la riga originale comunque.
    packet.packetType = PacketType::UNKNOWN;
    packet.label = "MALFORMED";
    packet.data = line;
    return packet;
}