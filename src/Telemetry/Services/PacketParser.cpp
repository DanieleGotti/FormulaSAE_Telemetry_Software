#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <algorithm>
#include "Telemetry/data_acquisition/PacketParser.hpp"

PacketParser::PacketParser() {
    resetCounters();
}

void PacketParser::resetCounters() {
    m_lastTimestampA = 0; m_lastTimestampB = 0;
    m_lostA = 0; m_lostB = 0;
    m_firstA = true; m_firstB = true;
    m_lockedA = 0; m_lockedB = 0;
    m_lastPacketBData.clear();
}

std::vector<std::string> PacketParser::getColumnOrder() {
    return TelemetrySchema::columnNames();
}

uint32_t PacketParser::readTimestamp(const uint8_t* data) {
    uint32_t ts = 0;
    std::memcpy(&ts, data + 2, 4);
    return ts;
}

//==============================================================================
//  Layout matching
//==============================================================================

const PacketLayout* PacketParser::matchLayout(const std::vector<PacketLayout>& layouts,
                                              int& lockedIndex,
                                              const uint8_t* data,
                                              size_t available) const {
    // The layout used for the previous packet is overwhelmingly likely to be
    // the right one again, so it is always tried first
    if (lockedIndex >= 0 && lockedIndex < static_cast<int>(layouts.size())) {
        if (TelemetrySchema::matches(layouts[lockedIndex], data, available)) {
            return &layouts[lockedIndex];
        }
    }
    for (size_t i = 0; i < layouts.size(); ++i) {
        if (static_cast<int>(i) == lockedIndex) continue;
        if (TelemetrySchema::matches(layouts[i], data, available)) {
            lockedIndex = static_cast<int>(i);
            std::cout << "INFO [PacketParser]: Locked on packet layout " << layouts[i].versionTag
                      << " (" << layouts[i].payloadSize << " bytes)." << std::endl;
            return &layouts[i];
        }
    }
    return nullptr;
}

//==============================================================================
//  Row population
//==============================================================================

void PacketParser::populateRowFromA(const PacketLayout& layout, const uint8_t* data, DbRow& row) {
    row["timestamp"] = TelemetrySchema::formatValue(ValueFormat::Number, 3, readTimestamp(data) * 0.005);

    for (size_t i = 0; i < layout.fields.size(); ++i) {
        const FieldDef& field = layout.fields[i];
        const double value = TelemetrySchema::readField(data, layout.fieldOffset(i), field);
        row[field.name] = TelemetrySchema::formatValue(field.format, field.decimals, value);
    }

    // Replay the most recent battery packet so every row is self contained
    for (const auto& entry : m_lastPacketBData) {
        row[entry.first] = entry.second;
    }

    row["lost_packets_A"]   = std::to_string(m_lostA);
    row["lost_packets_B"]   = std::to_string(m_lostB);
    row["lost_packets_tot"] = std::to_string(getTotalLost());
}

void PacketParser::populateRowFromB(const PacketLayout& layout, const uint8_t* data) {
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        const FieldDef& field = layout.fields[i];
        const double value = TelemetrySchema::readField(data, layout.fieldOffset(i), field);
        m_lastPacketBData[field.name] = TelemetrySchema::formatValue(field.format, field.decimals, value);
    }
    TelemetrySchema::expandDerivedKeys(m_lastPacketBData);
}

//==============================================================================
//  Stream decoding
//==============================================================================

bool PacketParser::parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow) {
    const auto& layoutsA = TelemetrySchema::layoutsA();
    const auto& layoutsB = TelemetrySchema::layoutsB();

    // Smallest and largest packet across every known layout, used to decide
    // whether a failed match means "corrupt" or simply "not enough bytes yet"
    static const size_t minA = std::min_element(layoutsA.begin(), layoutsA.end(),
        [](const PacketLayout& a, const PacketLayout& b){ return a.payloadSize < b.payloadSize; })->payloadSize;
    static const size_t maxA = std::max_element(layoutsA.begin(), layoutsA.end(),
        [](const PacketLayout& a, const PacketLayout& b){ return a.payloadSize < b.payloadSize; })->payloadSize;
    static const size_t minB = std::min_element(layoutsB.begin(), layoutsB.end(),
        [](const PacketLayout& a, const PacketLayout& b){ return a.payloadSize < b.payloadSize; })->payloadSize;
    static const size_t maxB = std::max_element(layoutsB.begin(), layoutsB.end(),
        [](const PacketLayout& a, const PacketLayout& b){ return a.payloadSize < b.payloadSize; })->payloadSize;

    while (rxBuffer.size() >= 2) {
        // ---- 1. Find the next plausible packet start ----
        size_t syncIndex = std::string::npos;
        int packetType = 0;

        for (size_t i = 0; i + 1 < rxBuffer.size(); ++i) {
            const size_t available = rxBuffer.size() - i;
            if (rxBuffer[i] == 0xCC && rxBuffer[i + 1] == 0x11 && available >= minA) {
                syncIndex = i; packetType = 1; break;
            }
            if (rxBuffer[i] == 0xEE && rxBuffer[i + 1] == 0x11 && available >= minB) {
                syncIndex = i; packetType = 2; break;
            }
        }
        if (syncIndex == std::string::npos) return false;

        // ---- 2. Report and drop anything preceding it ----
        if (syncIndex > 0) {
            const uint32_t currentTs = readTimestamp(rxBuffer.data() + syncIndex);
            const char* pktName = (packetType == 1) ? "0xCC" : "0xEE";

            std::cerr << "\n[WARNING] Finded " << syncIndex
                      << " bytes of garbage before the current packet [" << pktName
                      << " | Timestamp: " << currentTs << "].\n"
                      << "Discarded data: ";
            for (size_t k = 0; k < syncIndex; ++k) {
                std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                          << (int)rxBuffer[k] << " ";
            }
            std::cerr << std::dec << std::endl;

            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + syncIndex);
        }

        // ---- 3. Decode ----
        if (packetType == 1) {
            const PacketLayout* layoutA = matchLayout(layoutsA, m_lockedA, rxBuffer.data(), rxBuffer.size());

            if (!layoutA) {
                // Might still be a longer layout whose tail has not arrived yet
                if (rxBuffer.size() < maxA) return false;
                std::cerr << "\n[ERROR] Packet 0xCC corrupted (timestamp: "
                          << readTimestamp(rxBuffer.data()) << "): no known layout validates its CRC.\n" << std::endl;
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + 1);
                continue;
            }

            const uint32_t tsA = readTimestamp(rxBuffer.data());
            size_t consumed = layoutA->payloadSize;

            // Look ahead: when a battery packet follows immediately, decode it
            // first so the emitted row carries the freshest battery values.
            // Waiting for the two identifying bytes is what makes the result
            // independent of where the serial read happened to be cut.
            if (rxBuffer.size() < consumed + 2) return false;

            if (rxBuffer[consumed] == 0xEE && rxBuffer[consumed + 1] == 0x11) {

                if (rxBuffer.size() < consumed + minB) return false;

                const PacketLayout* layoutB = matchLayout(layoutsB, m_lockedB,
                                                          rxBuffer.data() + consumed,
                                                          rxBuffer.size() - consumed);
                if (!layoutB && rxBuffer.size() < consumed + maxB) return false;

                if (layoutB) {
                    const uint32_t tsB = readTimestamp(rxBuffer.data() + consumed);
                    if (!m_firstB && tsB > m_lastTimestampB) {
                        const uint32_t diff = tsB - m_lastTimestampB;
                        if (diff > 40 && diff < 100000) m_lostB += (diff / 40) - 1;
                    }
                    m_lastTimestampB = tsB;
                    m_firstB = false;

                    populateRowFromB(*layoutB, rxBuffer.data() + consumed);
                    consumed += layoutB->payloadSize;
                }
            }

            if (!m_firstA && tsA > m_lastTimestampA) {
                const uint32_t diff = tsA - m_lastTimestampA;
                if (diff > 1 && diff < 5000) m_lostA += (diff - 1);
            }
            m_lastTimestampA = tsA;
            m_firstA = false;

            outRow.clear();
            populateRowFromA(*layoutA, rxBuffer.data(), outRow);

            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + consumed);
            return true;

        } else {
            const PacketLayout* layoutB = matchLayout(layoutsB, m_lockedB, rxBuffer.data(), rxBuffer.size());

            if (!layoutB) {
                if (rxBuffer.size() < maxB) return false;
                std::cerr << "\n[ERROR] Packet 0xEE corrupted (timestamp: "
                          << readTimestamp(rxBuffer.data()) << "): no known layout validates its CRC.\n" << std::endl;
                rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + 1);
                continue;
            }

            const uint32_t tsB = readTimestamp(rxBuffer.data());
            if (!m_firstB && tsB > m_lastTimestampB) {
                const uint32_t diff = tsB - m_lastTimestampB;
                if (diff > 40 && diff < 100000) m_lostB += (diff / 40) - 1;
            }
            m_lastTimestampB = tsB;
            m_firstB = false;

            populateRowFromB(*layoutB, rxBuffer.data());
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + layoutB->payloadSize);
        }
    }
    return false;
}
