#pragma once
#include <string>
#include <variant>
#include <chrono>

using PacketData = std::variant<int, double, std::string>;

// Ordino i tipi di pacchetto in categorie
enum class PacketType {
    SENSOR_DATA,
    STATUS_LED,
    INVERTER,
    UNKNOWN
};

class PacketParser {
public:
    // Converte riga dato grezza in oggetto PacketParser
    static PacketParser parse(const std::string& line, const std::chrono::system_clock::time_point& reception_time);

    std::chrono::system_clock::time_point timestamp;
    PacketType packetType = PacketType::UNKNOWN;
    std::string label;
    PacketData data;

private:
    PacketParser() = default;
};

