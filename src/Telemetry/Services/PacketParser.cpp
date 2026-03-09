#include <sstream>
#include <unordered_set>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include "Telemetry/data_acquisition/PacketParser.hpp"

namespace {
    const std::unordered_set<std::string> integerSensorLabels = { "ACC1A", "ACC2A" };
    const std::unordered_set<std::string> doubleSensorLabels = {
        "ACC1B", "ACC2B", "BRK1", "BRK2", "STEER",
        "SOSPASX", "SOSPADX", "SOSPPSX", "SOSPPDX",
        "VELASX", "VELADX", "VELPDX", "VELPSX",
        "TMPDX", "TMPSX", "TMPMOTORDX", "TMPMOTORSX"
    };
    const std::unordered_set<std::string> statusLedLabels = { "SDC_INPUT", "RESET_BUTTON", "TS_ON_BUTTON", "R2D_BUTTON" };
    const std::unordered_set<std::string> inverterFsmLabels = { "LEFT_INVERTER_FSM", "RIGHT_INVERTER_FSM" };
    const std::map<int, std::string> inverterStateMapping = {
        {0, "OFF"}, {1, "SYSTEM_READY"}, {2, "DC_CAPACITOR_CHARGE"}, {3, "DC_OK"}, 
        {4, "ENABLE_INVERTER"}, {5, "ENABLED"}, {6, "ON"}, {7, "OK"}, {8, "ERROR"}
    };
    const std::string inverterStateKeyword = "STATE";
    bool isBatteryModuleLabel(const std::string& label) {
        return (label.rfind("TMP1M", 0) == 0) || 
               (label.rfind("TMP2M", 0) == 0) || 
               (label.rfind("TENSM", 0) == 0) || 
               (label.rfind("ERRORM", 0) == 0);
    }
}

std::chrono::system_clock::time_point PacketParser::parseTimestampString(const std::string& ts_str) {
    if (ts_str.length() < 23) return std::chrono::system_clock::time_point();

    const char* s = ts_str.data();

    auto parse_len =[](const char* p, int len) {
        int val = 0;
        for (int i = 0; i < len; ++i) {
            if (p[i] >= '0' && p[i] <= '9') {
                val = val * 10 + (p[i] - '0');
            }
        }
        return val;
    };

    // La cache salva in memoria i primi 19 caratteri (fino ai secondi)
    thread_local char cached_str[20] = {0}; 
    thread_local std::chrono::system_clock::time_point cached_tp;

    bool cache_hit = true;
    for (int i = 0; i < 19; ++i) {
        if (s[i] != cached_str[i]) {
            cache_hit = false;
            break;
        }
    }

    // Se il secondo è cambiato, usiamo mktime 
    if (!cache_hit) {
        for (int i = 0; i < 19; ++i) {
            cached_str[i] = s[i];
        }

        std::tm tm = {}; 
        tm.tm_year = parse_len(s, 4) - 1900;
        tm.tm_mon  = parse_len(s + 5, 2) - 1;
        tm.tm_mday = parse_len(s + 8, 2);
        tm.tm_hour = parse_len(s + 11, 2);
        tm.tm_min  = parse_len(s + 14, 2);
        tm.tm_sec  = parse_len(s + 17, 2);

        time_t time = std::mktime(&tm);
        if (time == -1) return std::chrono::system_clock::time_point(); 
        
        cached_tp = std::chrono::system_clock::from_time_t(time);
    }

    // Aggiunge i millisecondi 
    int ms = parse_len(s + 20, 3);
    return cached_tp + std::chrono::milliseconds(ms);
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
        if (isBatteryModuleLabel(label)) {
            packet.packetType = PacketType::SENSOR_DATA;
            packet.data = std::stod(value_str);
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