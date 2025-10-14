#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

class PacketParser {
public:
    PacketParser() = default;
    // Timestamp assegnato al momento di validazione
    std::chrono::system_clock::time_point timestamp;
    // Tipo di messaggio (es. "ACC1", "BRK1", ecc.)
    std::string type;  
    // Dato nel pacchetto
    std::vector<uint8_t> data;
};