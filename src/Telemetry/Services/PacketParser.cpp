#include "Telemetry/data_acquisition/PacketParser.hpp"
#include <sstream>
#include <unordered_set>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>

namespace {
    const std::unordered_set<std::string> integerSensorLabels = { "ACC1A", "ACC2A" };
    const std::unordered_set<std::string> doubleSensorLabels = {
        "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER",
        "SOSPASX", "SOSPADX", "SOSPPSX", "SOSPPDX",
        "VELASX", "VELADX", "VELPDX", "VELPSX"
    };
    const std::unordered_set<std::string> statusLedLabels = { "SDC_INPUT", "RESET_BUTTON", "TS_ON_BUTTON", "R2D_BUTTON" };
    const std::unordered_set<std::string> inverterFsmLabels = { "LEFT_INVERTER_FSM", "RIGHT_INVERTER_FSM" };
    const std::map<int, std::string> inverterStateMapping = {
        {0, "OFF"}, {1, "SYSTEM_READY"}, {2, "DC_CAPACITOR_CHARGE"}, {3, "DC_OK"}, 
        {4, "ENABLE_INVERTER"}, {5, "ENABLED"}, {6, "ON"}, {7, "OK"}, {8, "ERROR"}
    };
    const std::string inverterStateKeyword = "STATE";
}

std::chrono::system_clock::time_point PacketParser::parseTimestampString(const std::string& ts_str) {
    try {
        if (ts_str.length() < 23) return std::chrono::system_clock::time_point();
        std::tm tm = {};
        tm.tm_year = std::stoi(ts_str.substr(0, 4)) - 1900;
        tm.tm_mon = std::stoi(ts_str.substr(5, 2)) - 1;
        tm.tm_mday = std::stoi(ts_str.substr(8, 2));
        tm.tm_hour = std::stoi(ts_str.substr(11, 2));
        tm.tm_min = std::stoi(ts_str.substr(14, 2));
        tm.tm_sec = std::stoi(ts_str.substr(17, 2));
        time_t time = std::mktime(&tm);
        auto time_point = std::chrono::system_clock::from_time_t(time);
        int ms = std::stoi(ts_str.substr(20, 3));
        return time_point + std::chrono::milliseconds(ms);
    } catch (const std::exception&) {
        return std::chrono::system_clock::time_point();
    }
}

PacketParser PacketParser::parse(const std::string& line, const std::chrono::system_clock::time_point& reception_time, bool isFromFile) {
    PacketParser packet; 
    std::string label;
    std::string value_str;

    if (isFromFile) {
        std::stringstream ss(line);
        std::string timestamp_str;
        if (!std::getline(ss, timestamp_str, ';') || !std::getline(ss, label, ';') || !std::getline(ss, value_str)) {
             goto malformed;
        }
        packet.timestamp = PacketParser::parseTimestampString(timestamp_str);
        if (packet.timestamp.time_since_epoch().count() == 0) {
            goto malformed;
        }
    } else {
        packet.timestamp = reception_time;
        std::stringstream ss(line);
        std::string part1, part2, part3;
        ss >> part1 >> part2 >> part3;
        if (part1 == inverterStateKeyword && inverterFsmLabels.count(part2) && !part3.empty()) {
            label = part2; value_str = part3;
        } else if (!part2.empty() && part3.empty()) {
            label = part1; value_str = part2;
        } else {
            goto malformed;
        }
    }
    
    packet.label = label;
    try {
        if (integerSensorLabels.count(label)) {
            packet.packetType = PacketType::SENSOR_DATA;
            packet.data = std::stoi(value_str);
            return packet;
        }
        if (doubleSensorLabels.count(label)) {
            packet.packetType = PacketType::SENSOR_DATA;
            packet.data = std::stod(value_str);
            return packet;
        }
        if (statusLedLabels.count(label)) {
            packet.packetType = PacketType::STATUS_LED;
            packet.data = std::stoi(value_str);
            return packet;
        }
        if (inverterFsmLabels.count(label)) {
            packet.packetType = PacketType::INVERTER;
            if (isFromFile) {
                packet.data = value_str;
            } else {
                int stateValue = std::stoi(value_str);
                auto it = inverterStateMapping.find(stateValue);
                if (it != inverterStateMapping.end()) {
                    packet.data = it->second;
                } else {
                    packet.data = "UNKNOWN_STATE";
                }
            }
            return packet;
        }
    } catch (const std::exception&) { 
        // Errore di conversione, gestito da malformed sotto
    }
    
malformed:
    packet.packetType = PacketType::UNKNOWN;
    packet.label = isFromFile ? "MALFORMED_FROM_FILE" : "MALFORMED";
    packet.data = line;
    return packet;
}