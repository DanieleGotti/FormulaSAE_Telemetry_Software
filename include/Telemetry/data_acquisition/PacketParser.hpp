#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "TelemetrySchema.hpp"

//==============================================================================
//  Live packet decoder.
//
//  The wire layout is NOT hardcoded here: it comes from TelemetrySchema, which
//  also carries the historical layouts. The parser locks onto whichever layout
//  the connected ECU is actually transmitting, so an older board still works.
//==============================================================================

class PacketParser {
public:
    PacketParser();

    // Extracts packets from the incoming byte stream.
    // Returns true and fills outRow when a complete 5 ms packet has been decoded.
    bool parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow);

    void resetCounters();

    uint32_t getLostA() const { return m_lostA; }
    uint32_t getLostB() const { return m_lostB; }
    uint32_t getTotalLost() const { return m_lostA + m_lostB; }

    // Dataset/CSV column order, derived from the schema
    static std::vector<std::string> getColumnOrder();

private:
    // Returns the layout that validates at `data`, or nullptr when none does
    const PacketLayout* matchLayout(const std::vector<PacketLayout>& layouts,
                                    int& lockedIndex,
                                    const uint8_t* data,
                                    size_t available) const;

    void populateRowFromA(const PacketLayout& layout, const uint8_t* data, DbRow& row);
    void populateRowFromB(const PacketLayout& layout, const uint8_t* data);

    static uint32_t readTimestamp(const uint8_t* data);

    // Index of the layout currently in use, reused first on the next packet
    int m_lockedA = 0;
    int m_lockedB = 0;

    uint32_t m_lastTimestampA = 0;
    uint32_t m_lastTimestampB = 0;
    uint32_t m_lostA = 0;
    uint32_t m_lostB = 0;
    bool m_firstA = true;
    bool m_firstB = true;

    // Latest decoded 200 ms packet, replayed into every 5 ms row
    DbRow m_lastPacketBData;
};
