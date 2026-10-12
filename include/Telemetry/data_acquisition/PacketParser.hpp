/**
 * @file PacketParser.hpp
 * @brief Decoder of the packets received live from the serial port.
 *
 * @details
 * The packet layout is NOT hardcoded here: it comes from TelemetrySchema, which
 * also carries the historical layouts. The parser automatically locks onto the
 * layout the connected ECU is actually transmitting, so boards running an
 * older firmware still work.
 *
 * Flow: SerialService accumulates the raw bytes in a buffer and calls
 * parseLiveBytes() until it returns false; every successful call produces a
 * complete DbRow (one per 5 ms A packet) that is passed to DataAggregator.
 *
 * The parser is NOT thread-safe: it is used only by the SerialService
 * processing thread.
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "TelemetrySchema.hpp"

/**
 * @brief Extracts A (0xCC) and B (0xEE) packets from a byte stream and turns them into DbRow rows.
 *
 * It also keeps track of lost packets, computed from timestamp gaps: packet A
 * has a 1 tick (5 ms) period, packet B a 40 tick (200 ms) period.
 */
class PacketParser {
public:
    /** @brief Builds the parser with zeroed counters. */
    PacketParser();

    /**
     * @brief Extracts the next complete packet from the byte stream.
     *
     * Steps:
     *  1. find the next valid start sequence (0xCC 0x11 or 0xEE 0x11);
     *  2. drop (and report on stderr) any "garbage" bytes before it;
     *  3. validate the packet against the known layouts (start bytes + CRC);
     *  4. a B packet is stored and parsing continues; an A packet produces the row.
     *
     * If a B packet immediately follows an A packet, the B packet is decoded
     * BEFORE emitting the row, so the row carries the freshest battery values
     * regardless of where the serial read happened to cut the data.
     *
     * @param[in,out] rxBuffer Received bytes; consumed bytes are removed.
     * @param[out]    outRow   Produced row (valid only when the method returns true).
     * @return true if a row was produced (5 ms A packet decoded),
     *         false if more bytes are needed.
     */
    bool parseLiveBytes(std::vector<uint8_t>& rxBuffer, DbRow& outRow);

    /** @brief Resets lost counters, reference timestamps, locked layouts and the last B packet. */
    void resetCounters();

    /** @brief A packets (5 ms) lost since the last reset. */
    uint32_t getLostA() const { return m_lostA; }
    /** @brief B packets (200 ms) lost since the last reset. */
    uint32_t getLostB() const { return m_lostB; }
    /** @brief Sum of lost packets A + B. */
    uint32_t getTotalLost() const { return m_lostA + m_lostB; }

    /**
     * @brief Dataset/CSV column order, derived from the schema.
     * @return Same as TelemetrySchema::columnNames().
     */
    static std::vector<std::string> getColumnOrder();

private:
    /**
     * @brief Finds the layout that validates the bytes at `data`.
     *
     * The layout used for the previous packet (almost always the right one) is
     * tried first, then all the others. When a different layout is found,
     * `lockedIndex` is updated and the change is logged.
     *
     * @param layouts         Candidate layouts (TelemetrySchema::layoutsA() or layoutsB()).
     * @param[in,out] lockedIndex Index of the locked layout.
     * @param data            Start of the packet.
     * @param available       Bytes available from `data`.
     * @return The valid layout, or nullptr if none validates.
     */
    const PacketLayout* matchLayout(const std::vector<PacketLayout>& layouts,
                                    int& lockedIndex,
                                    const uint8_t* data,
                                    size_t available) const;

    /**
     * @brief Fills a row with timestamp, A packet fields, latest B values and counters.
     * @param layout Layout of the A packet.
     * @param data   Start of the A packet.
     * @param[out] row Row to fill.
     */
    void populateRowFromA(const PacketLayout& layout, const uint8_t* data, DbRow& row);
    /**
     * @brief Decodes a B packet into m_lastPacketBData (does not produce a row).
     * @param layout Layout of the B packet.
     * @param data   Start of the B packet.
     */
    void populateRowFromB(const PacketLayout& layout, const uint8_t* data);

    /**
     * @brief Reads the timestamp (u32 in 5 ms ticks) at offset 2 of the packet.
     * @param data Start of the packet.
     * @return Timestamp in ticks.
     */
    static uint32_t readTimestamp(const uint8_t* data);

    int m_lockedA = 0;  ///< Index of the A layout in use, tried first on the next packet.
    int m_lockedB = 0;  ///< Index of the B layout in use, tried first on the next packet.

    uint32_t m_lastTimestampA = 0; ///< Timestamp of the last A packet (ticks).
    uint32_t m_lastTimestampB = 0; ///< Timestamp of the last B packet (ticks).
    uint32_t m_lostA = 0;          ///< Lost A packets.
    uint32_t m_lostB = 0;          ///< Lost B packets.
    bool m_firstA = true;          ///< true until the first A packet arrives (no comparison possible).
    bool m_firstB = true;          ///< true until the first B packet arrives.

    /// Latest decoded B packet (200 ms), replayed into every 5 ms row.
    DbRow m_lastPacketBData;
};
