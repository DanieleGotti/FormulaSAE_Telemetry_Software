#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstddef>

//==============================================================================
//  SINGLE SOURCE OF TRUTH FOR THE TELEMETRY PROTOCOL
//
//  To add a new sensor you only touch TelemetrySchema.cpp:
//    1. append the field to the CURRENT layout table (kFieldsA or kFieldsB)
//    2. if the field must be visible as a statistic, add it to kDerivedColumns
//  Everything else (live parsing, dashboard table, CSV logging, .bin loading,
//  .csv loading, high-res export) is generated from these tables.
//
//  Old recordings stay readable: past layouts are kept in the historical tables
//  and are auto-detected, while .csv files are matched by column NAME.
//==============================================================================

using DbRow = std::map<std::string, std::string>;

// Representation of a value inside a raw packet
enum class WireType : uint8_t { U8, U16, I16, U32, F32 };

// How a value is rendered to, and parsed back from, text
enum class ValueFormat : uint8_t {
    Number,       // plain numeric, rendered with `decimals`
    EmmaError,    // bitmask -> "ERR_UNDERVOLTAGE | ERR_BSPD"
    InverterFsm,  // 0..8 -> state name
    TractiveFsm,  // 0..9 -> state name
    EcuMode,      // bitmask -> mode name
    InverterDiag  // AMK diagnostic number -> "2347 INVERTER_HW Motor temperature error"
};

size_t wireSize(WireType type);

// One field of a packet, and at the same time one column of the dataset
struct FieldDef {
    std::string name;
    WireType    wire     = WireType::F32;
    ValueFormat format   = ValueFormat::Number;
    double      scale    = 1.0;  // engineering value = raw * scale + bias
    double      bias     = 0.0;
    int         decimals = 1;
};

// A dataset column: either mirrors a packet field or is computed (stats, counters)
struct ColumnDef {
    std::string name;
    ValueFormat format   = ValueFormat::Number;
    int         decimals = 1;
};

// One historical wire layout of a packet type
struct PacketLayout {
    std::string           versionTag;
    uint8_t               startByte1 = 0;
    uint8_t               startByte2 = 0;
    std::vector<FieldDef> fields;      // laid out after the 4-byte timestamp
    std::vector<size_t>   offsets;     // byte offset of each field, precomputed
    size_t                payloadSize = 0;  // 2 start + 4 timestamp + fields + 2 CRC

    // Byte offset of `fields[i]` from the start of the packet
    inline size_t fieldOffset(size_t index) const { return offsets[index]; }
};

uint16_t Calculate_CRC16(const uint8_t* data, uint16_t length);

class TelemetrySchema {
public:
    // Known layouts, newest first. Index 0 is what the current ECU transmits.
    static const std::vector<PacketLayout>& layoutsA();
    static const std::vector<PacketLayout>& layoutsB();
    static const PacketLayout& currentA() { return layoutsA().front(); }
    static const PacketLayout& currentB() { return layoutsB().front(); }

    // Canonical dataset column order: timestamp, packet A, packet B, stats, counters
    static const std::vector<ColumnDef>& columns();
    static const std::vector<std::string>& columnNames();
    static int columnIndex(const std::string& name);  // -1 when unknown

    // Channels published as session-cumulative max/mean
    static const std::vector<std::string>& maxMeanChannels();

    // Vehicle speed is the mean of the two rear hall sensors, in km/h
    static constexpr double KMH_TO_MS = 1.0 / 3.6;
    // Longest gap still integrated into the distance: beyond this the samples
    // are considered missing rather than slow, and the interval is skipped
    static constexpr double MAX_DISTANCE_GAP_S = 1.0;
    // Channels published as windowed mean/variance/std
    static const std::vector<std::string>& varianceChannels();

    // Value <-> text, shared by live, logging, export and file loading so that
    // the same number always produces the very same string
    static std::string formatValue(ValueFormat format, int decimals, double value);
    static double      parseValue(ValueFormat format, const char* text);

    // Allocation-free variant used on the export hot path. Writes into `buffer`
    // and returns the number of characters produced.
    static size_t formatValueInto(ValueFormat format, int decimals, double value,
                                  char* buffer, size_t capacity);

    // Decode one field out of a raw packet buffer
    static double readField(const uint8_t* packet, size_t offset, const FieldDef& field);

    // Expand bitmask columns into the extra per-module keys the UI expects
    static void expandDerivedKeys(DbRow& row);

    // True when the packet at `data` validates against `layout` (start bytes + CRC)
    static bool matches(const PacketLayout& layout, const uint8_t* data, size_t available);
};
