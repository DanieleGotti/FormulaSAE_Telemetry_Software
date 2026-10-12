#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"
#include "Telemetry/data_acquisition/AmkDiagnostics.hpp"

//==============================================================================
//  Wire helpers
//==============================================================================

size_t wireSize(WireType type) {
    switch (type) {
        case WireType::U8:  return 1;
        case WireType::U16: return 2;
        case WireType::I16: return 2;
        case WireType::U32: return 4;
        case WireType::F32: return 4;
    }
    return 0;
}

// CRC-16/CCITT-FALSE, byte-at-a-time. The table holds the result of the
// original bit-by-bit loop for every possible byte, so the output is identical
// while costing eight times less work - which matters because loading a .bin
// now validates every single packet.
static const uint16_t* crcTable() {
    static const std::array<uint16_t, 256> table = [] {
        std::array<uint16_t, 256> t{};
        for (int i = 0; i < 256; ++i) {
            uint16_t crc = static_cast<uint16_t>(i << 8);
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                     : static_cast<uint16_t>(crc << 1);
            }
            t[i] = crc;
        }
        return t;
    }();
    return table.data();
}

uint16_t Calculate_CRC16(const uint8_t* data, uint16_t length) {
    static const uint16_t* table = crcTable();
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc = static_cast<uint16_t>((crc << 8) ^ table[((crc >> 8) ^ data[i]) & 0xFF]);
    }
    return crc;
}

//==============================================================================
//  Shared scaling constants
//==============================================================================

static constexpr double HALL_MS_TO_KMH   = 3.6;
static constexpr double HALL_ECU_SCALE   = 1.0;
static constexpr double HALL_SCALE       = HALL_MS_TO_KMH * HALL_ECU_SCALE;
static constexpr double DECI             = 0.1;   // raw stored multiplied by 10

//==============================================================================
//  FROZEN LAYOUT: packet 0xCC as transmitted before IMDInsulationValue (134 B).
//  Never edit this table - old recordings are decoded through it.
//==============================================================================

static std::vector<FieldDef> fieldsA_v2() {
    return {
        { "front_left_velocity",       WireType::F32, ValueFormat::Number, HALL_SCALE, 0.0, 1 },
        { "front_right_velocity",      WireType::F32, ValueFormat::Number, HALL_SCALE, 0.0, 1 },
        { "rear_left_velocity",        WireType::F32, ValueFormat::Number, HALL_SCALE, 0.0, 1 },
        { "rear_right_velocity",       WireType::F32, ValueFormat::Number, HALL_SCALE, 0.0, 1 },

        { "front_left_suspension",     WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },
        { "front_right_suspension",    WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },

        { "accelerator1",              WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 },
        { "accelerator2",              WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 },
        { "accelerator_mapped",        WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },

        { "brake1",                    WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 },
        { "brake2",                    WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 },

        { "steer",                     WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },

        { "sdc",                       WireType::U8,  ValueFormat::Number, 1.0, 0.0, 0 },
        { "ready_to_drive_button",     WireType::U8,  ValueFormat::Number, 1.0, 0.0, 0 },
        { "ecu_reset_button",          WireType::U8,  ValueFormat::Number, 1.0, 0.0, 0 },
        { "tractive_system_on_button", WireType::U8,  ValueFormat::Number, 1.0, 0.0, 0 },

        { "emma_current",              WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },
        { "emma_voltage",              WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },
        { "emma_yaw",                  WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "emma_error",                WireType::U16, ValueFormat::EmmaError, 1.0, 0.0, 0 },

        { "mean_velocity",             WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },
        { "real_yaw_rate",             WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "total_torque_request",      WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "torque_tv_L",               WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "torque_tv_R",               WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "slip_L",                    WireType::F32, ValueFormat::Number, 1.0, 0.0, 3 },
        { "slip_R",                    WireType::F32, ValueFormat::Number, 1.0, 0.0, 3 },
        { "torque_reduction_L",        WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "torque_reduction_R",        WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "final_torque_target_L",     WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },
        { "final_torque_target_R",     WireType::F32, ValueFormat::Number, 1.0, 0.0, 2 },

        { "inv_L_torqueCurrent",       WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },
        { "inv_L_magnetizingCurrent",  WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },
        { "inv_L_tempMotor",           WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },
        { "inv_R_torqueCurrent",       WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },
        { "inv_R_magnetizingCurrent",  WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },
        { "inv_R_tempMotor",           WireType::I16, ValueFormat::Number, DECI, 0.0, 1 },

        { "left_coolant_temp",         WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },
        { "right_coolant_temp",        WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 },

        { "left_inverter_fsm",         WireType::U8,  ValueFormat::InverterFsm, 1.0, 0.0, 0 },
        { "right_inverter_fsm",        WireType::U8,  ValueFormat::InverterFsm, 1.0, 0.0, 0 },
        { "tractive_system_fsm",       WireType::U8,  ValueFormat::TractiveFsm, 1.0, 0.0, 0 },

        { "ECU_Mode",                  WireType::U8,  ValueFormat::EcuMode, 1.0, 0.0, 0 },
    };
}

//==============================================================================
//  FROZEN LAYOUT: the oldest packet 0xCC, which carried two dedicated motor
//  temperatures between the inverter block and the coolant block (142 B).
//==============================================================================

static std::vector<FieldDef> fieldsA_v1() {
    std::vector<FieldDef> fields = fieldsA_v2();
    const FieldDef leftMotor  { "left_motor_temp",  WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 };
    const FieldDef rightMotor { "right_motor_temp", WireType::F32, ValueFormat::Number, 1.0, 0.0, 1 };
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == "left_coolant_temp") {
            fields.insert(fields.begin() + i, { leftMotor, rightMotor });
            break;
        }
    }
    return fields;
}

//==============================================================================
//  FROZEN LAYOUT: adds the IMD insulation reading (136 B).
//==============================================================================

static std::vector<FieldDef> fieldsA_v3() {
    std::vector<FieldDef> fields = fieldsA_v2();
    fields.push_back({ "IMDInsulationValue", WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 });
    return fields;
}

//==============================================================================
//  FROZEN LAYOUT: adds the two AMK diagnostic numbers, one per inverter (140 B).
//==============================================================================

static std::vector<FieldDef> fieldsA_v4() {
    std::vector<FieldDef> fields = fieldsA_v3();
    fields.push_back({ "inv_L_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });
    fields.push_back({ "inv_R_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });
    return fields;
}

//==============================================================================
//  >>>>>>>>>>>>>>>>>>  ADD NEW SENSORS OF PACKET 0xCC HERE  <<<<<<<<<<<<<<<<<<
//
//  This is the layout the ECU transmits TODAY. Append one entry, in the exact
//  position the firmware sends it, and it automatically appears in the live UI,
//  the dashboard table, the CSV log, the .bin loader and the high-res export.
//
//  The frozen tables above are untouched, so previous recordings keep working.
//==============================================================================

static std::vector<FieldDef> fieldsA_current() {
    std::vector<FieldDef> fields = fieldsA_v3();

    // Numero di diagnostica AMK per inverter (0 = nessuna). Vedi AmkDiagnostics.hpp
    fields.push_back({ "inv_L_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });
    fields.push_back({ "inv_R_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });

    // fields.push_back({ "brake_line_pressure", WireType::U16, ValueFormat::Number, 0.1, 0.0, 1 });

    return fields;
}

//==============================================================================
//  FROZEN LAYOUT: packet 0xEE before the online module bitmask (79 B).
//==============================================================================

static std::vector<FieldDef> fieldsB_v1() {
    std::vector<FieldDef> fields;
    fields.push_back({ "state_of_charge", WireType::U8, ValueFormat::Number, 1.0, 0.0, 0 });

    // Cell voltages: raw is stored as (V * 100) - 200 to fit one byte
    for (int i = 1; i <= 14; ++i) {
        fields.push_back({ "VoltageModule" + std::to_string(i), WireType::U8, ValueFormat::Number, 0.01, 2.0, 2 });
    }
    // Two temperature probes per module, interleaved on the wire
    for (int i = 1; i <= 14; ++i) {
        fields.push_back({ "Temperature1Module" + std::to_string(i), WireType::U16, ValueFormat::Number, DECI, 0.0, 1 });
        fields.push_back({ "Temperature2Module" + std::to_string(i), WireType::U16, ValueFormat::Number, DECI, 0.0, 1 });
    }
    return fields;
}

//==============================================================================
//  >>>>>>>>>>>>>>>>>>  ADD NEW SENSORS OF PACKET 0xEE HERE  <<<<<<<<<<<<<<<<<<
//  Same rule as packet 0xCC: only this function is edited.
//==============================================================================

static std::vector<FieldDef> fieldsB_current() {
    std::vector<FieldDef> fields = fieldsB_v1();
    fields.push_back({ "online_modules", WireType::U16, ValueFormat::Number, 1.0, 0.0, 0 });
    return fields;
}

//==============================================================================
//  Historical layouts, kept so that old recordings remain readable.
//  Never edit these: add a new entry at the top of the list instead.
//==============================================================================

static PacketLayout makeLayout(const std::string& tag, uint8_t s1, uint8_t s2, std::vector<FieldDef> fields) {
    PacketLayout layout;
    layout.versionTag = tag;
    layout.startByte1 = s1;
    layout.startByte2 = s2;
    layout.fields = std::move(fields);

    // Offsets are resolved once here so decoding a packet stays a flat walk
    size_t offset = 6; // 2 start bytes + 4 timestamp bytes
    layout.offsets.reserve(layout.fields.size());
    for (const auto& f : layout.fields) {
        layout.offsets.push_back(offset);
        offset += wireSize(f.wire);
    }
    layout.payloadSize = offset + 2; // + CRC
    return layout;
}

// Two layouts are the same when they describe the same fields in the same
// order. Used to avoid registering a duplicate while the current layout still
// matches the newest frozen one.
static bool sameFields(const std::vector<FieldDef>& a, const std::vector<FieldDef>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].wire != b[i].wire) return false;
    }
    return true;
}

// Registers `candidate` unless an identical layout is already listed
static void addLayout(std::vector<PacketLayout>& list, const std::string& tag,
                      uint8_t s1, uint8_t s2, std::vector<FieldDef> fields) {
    for (const auto& existing : list) {
        if (sameFields(existing.fields, fields)) return;
    }
    list.push_back(makeLayout(tag, s1, s2, std::move(fields)));
}

const std::vector<PacketLayout>& TelemetrySchema::layoutsA() {
    static const std::vector<PacketLayout> layouts = [] {
        // Newest first: detection tries them in this order
        std::vector<PacketLayout> list;
        addLayout(list, "A/current",           0xCC, 0x11, fieldsA_current());
        addLayout(list, "A/v4 (inverter diag)", 0xCC, 0x11, fieldsA_v4());
        addLayout(list, "A/v3 (IMD)",          0xCC, 0x11, fieldsA_v3());
        addLayout(list, "A/v2 (no IMD)",       0xCC, 0x11, fieldsA_v2());
        addLayout(list, "A/v1 (motor temps)",  0xCC, 0x11, fieldsA_v1());
        return list;
    }();
    return layouts;
}

const std::vector<PacketLayout>& TelemetrySchema::layoutsB() {
    static const std::vector<PacketLayout> layouts = [] {
        std::vector<PacketLayout> list;
        addLayout(list, "B/current",           0xEE, 0x11, fieldsB_current());
        addLayout(list, "B/v1 (no online)",    0xEE, 0x11, fieldsB_v1());
        return list;
    }();
    return layouts;
}

//==============================================================================
//  Derived columns and canonical dataset order
//==============================================================================

const std::vector<std::string>& TelemetrySchema::varianceChannels() {
    static const std::vector<std::string> channels = {
        "accelerator1", "accelerator2", "brake1", "brake2", "steer"
    };
    return channels;
}

const std::vector<std::string>& TelemetrySchema::maxMeanChannels() {
    static const std::vector<std::string> channels = {
        "speed", "inv_L_tempMotor", "inv_R_tempMotor", "left_coolant_temp", "right_coolant_temp"
    };
    return channels;
}

const std::vector<ColumnDef>& TelemetrySchema::columns() {
    static const std::vector<ColumnDef> cols = [] {
        std::vector<ColumnDef> list;
        list.push_back({ "timestamp", ValueFormat::Number, 3 });

        // Post-processed: distance travelled, integrated from the speedometer
        // reading over the session timestamps. Sits right after the timestamp
        // because the two are always read together.
        list.push_back({ "meters", ValueFormat::Number, 1 });

        for (const auto& f : currentA().fields) list.push_back({ f.name, f.format, f.decimals });
        for (const auto& f : currentB().fields) list.push_back({ f.name, f.format, f.decimals });

        for (const auto& sensor : varianceChannels()) {
            list.push_back({ sensor + "_mean", ValueFormat::Number, 3 });
            list.push_back({ sensor + "_var",  ValueFormat::Number, 3 });
            list.push_back({ sensor + "_std",  ValueFormat::Number, 3 });
        }
        for (const auto& channel : maxMeanChannels()) {
            list.push_back({ channel + "_max",  ValueFormat::Number, 3 });
            list.push_back({ channel + "_mean", ValueFormat::Number, 3 });
        }

        list.push_back({ "lost_packets_A",   ValueFormat::Number, 0 });
        list.push_back({ "lost_packets_B",   ValueFormat::Number, 0 });
        list.push_back({ "lost_packets_tot", ValueFormat::Number, 0 });
        return list;
    }();
    return cols;
}

const std::vector<std::string>& TelemetrySchema::columnNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list;
        list.reserve(columns().size());
        for (const auto& c : columns()) list.push_back(c.name);
        return list;
    }();
    return names;
}

int TelemetrySchema::columnIndex(const std::string& name) {
    static const std::unordered_map<std::string, int> lookup = [] {
        std::unordered_map<std::string, int> map;
        const auto& cols = columns();
        for (size_t i = 0; i < cols.size(); ++i) map[cols[i].name] = static_cast<int>(i);
        return map;
    }();
    const auto it = lookup.find(name);
    return it == lookup.end() ? -1 : it->second;
}

//==============================================================================
//  Enumerated value tables
//==============================================================================

namespace {

struct FlagEntry { uint16_t bit; const char* name; };

const FlagEntry kEmmaFlags[] = {
    { 0x0001, "ERR_UNDERVOLTAGE" },     { 0x0002, "ERR_OVERVOLTAGE" },
    { 0x0004, "ERR_OVERTEMP_CELL" },    { 0x0008, "ERR_UNDERTEMP_CELL" },
    { 0x0010, "ERR_OVERTEMP_PCB" },     { 0x0020, "ERR_UNDERTEMP_PCB" },
    { 0x0040, "ERR_CELL_MISMATCH" },    { 0x0080, "ERR_BSPD" },
    { 0x0100, "ERR_DAISY_CHAIN" },      { 0x0200, "IMD_AMS_LATCH" },
    { 0x0800, "EMMA_FINISH_PRECHARGE" },{ 0x8000, "ERR_CRITICAL_FAULT" }
};

const char* const kInverterStates[] = {
    "OFF", "SYSTEM_READY", "DC_CAPACITOR_CHARGE", "DC_OK",
    "ENABLE_INVERTER", "ENABLED", "ON", "OK", "ERROR"
};

const char* const kTractiveStates[] = {
    "ENTRY_STATE", "START_LIGHT_TEST", "WAIT_LIGHT_TEST", "POWER_OFF",
    "TS_WAIT_ACTIVATION", "TS_WAIT_PRECHARGE", "TS_ACTIVE", "R2D_WAIT",
    "R2D_ACTIVE", "ERROR_STATE"
};

const FlagEntry kEcuModes[] = {
    { 0x01, "ENDURANCE" }, { 0x02, "ACCELERATION" }, { 0x04, "TEST" }
};

std::string emmaErrorToText(uint16_t error) {
    if (error == 0) return "EMMA_STATUS_OK";
    std::string result;
    for (const auto& flag : kEmmaFlags) {
        if (error & flag.bit) {
            if (!result.empty()) result += " | ";
            result += flag.name;
        }
    }
    return result.empty() ? "UNKNOWN" : result;
}

// Exact reverse of emmaErrorToText, so a value survives the CSV round trip
uint16_t emmaErrorFromText(const char* text) {
    if (!text || *text == '\0') return 0;
    if (std::strstr(text, "EMMA_STATUS_OK")) return 0;
    uint16_t error = 0;
    for (const auto& flag : kEmmaFlags) {
        if (std::strstr(text, flag.name)) error |= flag.bit;
    }
    return error;
}

// Exact match against the state table; falls back to a plain number so that a
// column already stored numerically still loads correctly
double enumFromText(const char* text, const char* const* table, size_t count) {
    if (!text || *text == '\0') return 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(text, table[i]) == 0) return static_cast<double>(i);
    }
    if (text[0] >= '0' && text[0] <= '9') return std::atof(text);
    return 0.0;
}

} // namespace

//==============================================================================
//  Formatting and parsing
//==============================================================================

std::string TelemetrySchema::formatValue(ValueFormat format, int decimals, double value) {
    switch (format) {
        case ValueFormat::EmmaError:
            return emmaErrorToText(static_cast<uint16_t>(value));

        case ValueFormat::InverterFsm: {
            const int index = static_cast<int>(value);
            const int count = static_cast<int>(sizeof(kInverterStates) / sizeof(kInverterStates[0]));
            return (index >= 0 && index < count) ? kInverterStates[index] : "UNKNOWN";
        }
        case ValueFormat::TractiveFsm: {
            const int index = static_cast<int>(value);
            const int count = static_cast<int>(sizeof(kTractiveStates) / sizeof(kTractiveStates[0]));
            return (index >= 0 && index < count) ? kTractiveStates[index] : "UNKNOWN";
        }
        case ValueFormat::EcuMode: {
            const uint8_t raw = static_cast<uint8_t>(value);
            for (const auto& mode : kEcuModes) {
                if (raw & mode.bit) return mode.name;
            }
            return "UNKNOWN";
        }
        case ValueFormat::InverterDiag:
            return amkDiagToText(static_cast<uint16_t>(value));
        case ValueFormat::Number:
        default: {
            // snprintf on the default "C" locale: always a dot separator, and
            // an order of magnitude cheaper than building an ostringstream
            char buffer[64];
            const int written = std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
            return std::string(buffer, written > 0 ? static_cast<size_t>(written) : 0);
        }
    }
}

size_t TelemetrySchema::formatValueInto(ValueFormat format, int decimals, double value,
                                        char* buffer, size_t capacity) {
    if (format == ValueFormat::Number) {
        const int written = std::snprintf(buffer, capacity, "%.*f", decimals, value);
        return written > 0 ? static_cast<size_t>(written) : 0;
    }
    // Enumerated values are short strings; reuse the shared table lookup
    const std::string text = formatValue(format, decimals, value);
    const size_t length = std::min(text.size(), capacity ? capacity - 1 : 0);
    std::memcpy(buffer, text.data(), length);
    buffer[length] = '\0';
    return length;
}

double TelemetrySchema::parseValue(ValueFormat format, const char* text) {
    switch (format) {
        case ValueFormat::EmmaError:
            return static_cast<double>(emmaErrorFromText(text));

        case ValueFormat::InverterFsm:
            return enumFromText(text, kInverterStates, sizeof(kInverterStates) / sizeof(kInverterStates[0]));

        case ValueFormat::TractiveFsm:
            return enumFromText(text, kTractiveStates, sizeof(kTractiveStates) / sizeof(kTractiveStates[0]));

        case ValueFormat::EcuMode: {
            if (!text || *text == '\0') return 0.0;
            for (const auto& mode : kEcuModes) {
                if (std::strcmp(text, mode.name) == 0) return static_cast<double>(mode.bit);
            }
            return 0.0;
        }
        case ValueFormat::InverterDiag:
            return static_cast<double>(amkDiagFromText(text));

        case ValueFormat::Number:
        default:
            return text ? std::atof(text) : 0.0;
    }
}

//==============================================================================
//  Raw packet decoding
//==============================================================================

double TelemetrySchema::readField(const uint8_t* packet, size_t offset, const FieldDef& field) {
    const uint8_t* p = packet + offset;
    double raw = 0.0;

    switch (field.wire) {
        case WireType::U8:  raw = static_cast<double>(*p); break;
        case WireType::U16: { uint16_t v; std::memcpy(&v, p, 2); raw = static_cast<double>(v); break; }
        case WireType::I16: { int16_t  v; std::memcpy(&v, p, 2); raw = static_cast<double>(v); break; }
        case WireType::U32: { uint32_t v; std::memcpy(&v, p, 4); raw = static_cast<double>(v); break; }
        case WireType::F32: { float    v; std::memcpy(&v, p, 4); raw = static_cast<double>(v); break; }
    }
    return raw * field.scale + field.bias;
}

bool TelemetrySchema::matches(const PacketLayout& layout, const uint8_t* data, size_t available) {
    if (available < layout.payloadSize) return false;
    if (data[0] != layout.startByte1 || data[1] != layout.startByte2) return false;

    const uint16_t expected = Calculate_CRC16(data, static_cast<uint16_t>(layout.payloadSize - 2));
    uint16_t stored = 0;
    std::memcpy(&stored, data + layout.payloadSize - 2, 2);
    return expected == stored;
}

void TelemetrySchema::expandDerivedKeys(DbRow& row) {
    // The battery window reads one key per module rather than the raw bitmask
    const auto it = row.find("online_modules");
    if (it == row.end()) return;

    const uint16_t mask = static_cast<uint16_t>(std::atoi(it->second.c_str()));
    for (int i = 0; i < 14; ++i) {
        row["OnlineModule" + std::to_string(i + 1)] = ((mask & (1 << i)) != 0) ? "1" : "0";
    }
}
