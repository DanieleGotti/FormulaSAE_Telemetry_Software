/**
 * @file TelemetrySchema.hpp
 * @brief Single source of truth for the telemetry protocol (packet layouts and dataset columns).
 *
 * @details
 * This header describes HOW the packets sent by the ECU are laid out and HOW
 * their values become dataset columns. Everything else in the software (live
 * parser, Dashboard table, CSV logging, .bin/.csv loading, high-res export) is
 * generated from these tables: no other file holds the list of fields.
 *
 * ## Packet format on the wire (little-endian)
 * @code
 *  offset 0      1      2..5            6 .. N-3               N-2..N-1
 *       +------+------+---------------+----------------------+---------+
 *       | SB1  | SB2  | timestamp u32 | fields (FieldDef...) | CRC16   |
 *       +------+------+---------------+----------------------+---------+
 *  SB1/SB2   = start bytes (0xCC 0x11 packet A, 0xEE 0x11 packet B)
 *  timestamp = ECU counter in 5 ms ticks (seconds = ticks * 0.005)
 *  CRC16     = CRC-16/CCITT-FALSE computed over all the preceding bytes
 * @endcode
 *
 * - **Packet A (0xCC 0x11)**: fast data, sent every 5 ms (200 Hz).
 * - **Packet B (0xEE 0x11)**: battery data (EMMA), sent every 200 ms.
 *   Every dataset row is produced by an A packet; the latest B packet values
 *   are "replayed" into every following A row.
 *
 * ## How to ADD a sensor (only TelemetrySchema.cpp is touched)
 * 1. If the current layout (`fieldsA_current()` / `fieldsB_current()`) has
 *    already been used to record data, freeze it FIRST: create a new function
 *    `fieldsA_vN()` identical to the current layout and register it in
 *    TelemetrySchema::layoutsA() (below "A/current"). This keeps old
 *    recordings readable.
 * 2. Append the field at the end of `fieldsA_current()` (or `fieldsB_current()`)
 *    IN THE SAME POSITION the firmware sends it, specifying the wire type
 *    (WireType), the text format (ValueFormat), scale, bias and number of decimals.
 * 3. (Optional) If the value needs statistics, add its name to
 *    varianceChannels() (mean/variance/std over 3 s windows) or to
 *    maxMeanChannels() (session-cumulative max/mean).
 * 4. The field automatically shows up in the Dashboard "Current data" table,
 *    in the CSV log, in .bin/.csv loading and in the export. To show it in a
 *    dedicated window see the guide in UIManager.hpp.
 * 5. Update the Python simulators too (tester_uart/data_gen.py and
 *    tester_bin_sd/main.py) so the software can be tested without the car.
 *
 * ## How to REMOVE or CHANGE a sensor
 * - Freeze the current layout (step 1 above), then remove/change the field only
 *   in `fieldsA_current()`. NEVER touch the `fieldsX_vN()` functions.
 * - Search for the field name (string) in `src/UI` and `src/Telemetry`: windows
 *   reading it will simply show "Null" / "--" when it is missing.
 * - Note: automatic layout detection relies on length and CRC. Two layouts with
 *   the SAME total length but different fields cannot be told apart: the first
 *   one in the list (the newest) wins.
 *
 * @see TelemetrySchema.cpp for the field tables.
 * @see PacketParser (live decoding), FileService (file decoding).
 */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstddef>

/**
 * @brief One telemetry row in text form: column name -> already formatted value.
 *
 * This is the exchange format between backend and UI. Values are strings
 * produced by TelemetrySchema::formatValue(), so windows must convert them
 * (e.g. `std::stod`) when they need numbers. A missing key means "data not
 * available" (channel not present in the layout/recording).
 */
using DbRow = std::map<std::string, std::string>;

/**
 * @brief Binary type a value is transmitted with inside the packet.
 *
 * Determines how many bytes it takes (see wireSize()) and how it is read
 * (TelemetrySchema::readField()). All types are little-endian.
 */
enum class WireType : uint8_t { U8, U16, I16, U32, F32 };

/**
 * @brief How a value is converted to text (and parsed back from text).
 *
 * Every non-numeric format needs a lookup table in TelemetrySchema.cpp and a
 * branch in formatValue()/parseValue(). To add a new enumerated type: add the
 * value here, the name table and both conversion branches (the two directions
 * must be exact inverses, otherwise a saved CSV does not reload correctly).
 */
enum class ValueFormat : uint8_t {
    Number,       ///< Plain number, printed with `decimals` decimal digits.
    EmmaError,    ///< EMMA error bitmask -> "ERR_UNDERVOLTAGE | ERR_BSPD".
    InverterFsm,  ///< Inverter FSM state 0..8 -> state name.
    TractiveFsm,  ///< Tractive system FSM state 0..9 -> state name.
    EcuMode,      ///< ECU mode bitmask -> mode name.
    InverterDiag  ///< AMK diagnostic number -> "2347 INVERTER_HW Motor temperature error".
};

/**
 * @brief Number of bytes a type takes on the wire.
 * @param type Binary type.
 * @return Size in bytes (1, 2 or 4), 0 for an unknown type.
 */
size_t wireSize(WireType type);

/**
 * @brief One packet field and, at the same time, one dataset column.
 *
 * The engineering value is computed as `raw * scale + bias`, where `raw` is the
 * number read from the packet according to `wire`.
 */
struct FieldDef {
    std::string name;                          ///< Column name (DbRow key and CSV header). Must be unique.
    WireType    wire     = WireType::F32;      ///< Binary type on the wire.
    ValueFormat format   = ValueFormat::Number; ///< Text format of the value.
    double      scale    = 1.0;                ///< Multiplier: value = raw * scale + bias.
    double      bias     = 0.0;                ///< Offset added after scaling.
    int         decimals = 1;                  ///< Decimal digits used when printing (ValueFormat::Number only).
};

/**
 * @brief One dataset column: a copy of a packet field or a computed value.
 *
 * Computed columns (statistics, distance, lost packet counters) do not exist
 * in the packet: they are produced by DataAggregator (live) or by
 * FileService::computeMissingStats() (files).
 */
struct ColumnDef {
    std::string name;                          ///< Column name.
    ValueFormat format   = ValueFormat::Number; ///< Text format.
    int         decimals = 1;                  ///< Decimal digits when printing.
};

/**
 * @brief One of the layouts (current or historical) of a packet type.
 *
 * Built once by `makeLayout()` in TelemetrySchema.cpp, which precomputes the
 * offset of every field and the total size.
 */
struct PacketLayout {
    std::string           versionTag;      ///< Human-readable tag (e.g. "A/current", "A/v3 (IMD)"), used in logs.
    uint8_t               startByte1 = 0;  ///< First sync byte (0xCC for A, 0xEE for B).
    uint8_t               startByte2 = 0;  ///< Second sync byte (0x11).
    std::vector<FieldDef> fields;      ///< Fields in transmission order, after the 4 timestamp bytes.
    std::vector<size_t>   offsets;     ///< Byte offset of each field from the start of the packet (precomputed).
    size_t                payloadSize = 0;  ///< Total size: 2 start + 4 timestamp + fields + 2 CRC.

    /**
     * @brief Offset of `fields[index]` from the start of the packet.
     * @param index Index of the field in `fields`.
     * @return Offset in bytes.
     */
    inline size_t fieldOffset(size_t index) const { return offsets[index]; }
};

/**
 * @brief Computes the CRC-16/CCITT-FALSE (polynomial 0x1021, init 0xFFFF).
 *
 * Must match the ECU firmware implementation and `calculate_crc16()` in the
 * Python simulators.
 *
 * @param data   Pointer to the bytes to check.
 * @param length Number of bytes.
 * @return The computed CRC.
 */
uint16_t Calculate_CRC16(const uint8_t* data, uint16_t length);

/**
 * @brief Static access to the schema: packet layouts, columns, formatting.
 *
 * All methods are static and tables are built lazily on first access (local
 * statics), so they are thread-safe for reading.
 */
class TelemetrySchema {
public:
    /**
     * @brief Known layouts of packet A (0xCC), newest first.
     * @return Layout list; index 0 is what the current ECU transmits.
     */
    static const std::vector<PacketLayout>& layoutsA();
    /**
     * @brief Known layouts of packet B (0xEE), newest first.
     * @return Layout list; index 0 is what the current ECU transmits.
     */
    static const std::vector<PacketLayout>& layoutsB();
    /** @brief Current layout of packet A (the one that defines the columns). */
    static const PacketLayout& currentA() { return layoutsA().front(); }
    /** @brief Current layout of packet B (the one that defines the columns). */
    static const PacketLayout& currentB() { return layoutsB().front(); }

    /**
     * @brief Canonical dataset column order.
     *
     * Order: `timestamp`, `meters`, packet A fields, packet B fields,
     * statistics (variance then max/mean), lost packet counters.
     * This is the order used for the CSV log header and the Dashboard table.
     */
    static const std::vector<ColumnDef>& columns();
    /** @brief Just the names of columns(), in the same order. */
    static const std::vector<std::string>& columnNames();
    /**
     * @brief Index of a column in columns().
     * @param name Column name.
     * @return Index, or -1 if the column does not exist.
     */
    static int columnIndex(const std::string& name);

    /**
     * @brief Channels published as session-cumulative max and mean.
     *
     * Each one gets the columns `<name>_max` and `<name>_mean`.
     * "speed" is a virtual channel: the mean of the two rear wheels.
     */
    static const std::vector<std::string>& maxMeanChannels();

    /// km/h -> m/s conversion factor (vehicle speed is in km/h).
    static constexpr double KMH_TO_MS = 1.0 / 3.6;
    /// Longest gap (s) between two samples still integrated into the distance:
    /// beyond this the samples are considered lost and the interval is skipped.
    static constexpr double MAX_DISTANCE_GAP_S = 1.0;
    /**
     * @brief Channels published as mean, variance and standard deviation over 3 s windows.
     *
     * Each one gets the columns `<name>_mean`, `<name>_var`, `<name>_std`.
     */
    static const std::vector<std::string>& varianceChannels();

    /**
     * @brief Converts a numeric value into the text shown in the UI and written to CSVs.
     *
     * Shared by live, logging, export and file loading: the same number always
     * produces the very same string.
     *
     * @param format   Value format.
     * @param decimals Decimal digits (ValueFormat::Number only).
     * @param value    Engineering value.
     * @return Formatted text (dot as decimal separator).
     */
    static std::string formatValue(ValueFormat format, int decimals, double value);
    /**
     * @brief Inverse of formatValue(): parses a value back from text (e.g. from a CSV).
     * @param format Column format.
     * @param text   Text to parse (may be nullptr or empty -> 0).
     * @return Numeric value.
     */
    static double      parseValue(ValueFormat format, const char* text);

    /**
     * @brief Allocation-free variant of formatValue(), used on the fast export path.
     * @param format   Value format.
     * @param decimals Decimal digits.
     * @param value    Value.
     * @param buffer   Destination buffer ('\0' terminated).
     * @param capacity Buffer size.
     * @return Number of characters written (terminator excluded).
     */
    static size_t formatValueInto(ValueFormat format, int decimals, double value,
                                  char* buffer, size_t capacity);

    /**
     * @brief Decodes one field from a raw packet, applying scale and bias.
     * @param packet Pointer to the start of the packet (start bytes).
     * @param offset Field offset (PacketLayout::fieldOffset()).
     * @param field  Field definition.
     * @return Engineering value.
     */
    static double readField(const uint8_t* packet, size_t offset, const FieldDef& field);

    /**
     * @brief Expands bitmask columns into the extra keys the UI expects.
     *
     * Today: `online_modules` -> `OnlineModule1` .. `OnlineModule14` ("1"/"0"),
     * read by the battery window.
     *
     * @param row Row to enrich (modified in place).
     */
    static void expandDerivedKeys(DbRow& row);

    /**
     * @brief Checks whether the bytes at `data` are a valid packet for `layout`.
     * @param layout    Candidate layout.
     * @param data      Start of the possible packet.
     * @param available Bytes available from `data`.
     * @return true if there are enough bytes, the start bytes match and the CRC is correct.
     */
    static bool matches(const PacketLayout& layout, const uint8_t* data, size_t available);
};
