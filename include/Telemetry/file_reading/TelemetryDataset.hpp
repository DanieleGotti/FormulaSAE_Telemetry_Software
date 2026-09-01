#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"

//==============================================================================
//  Column-agnostic, cache friendly storage for a loaded session.
//
//  Values live in ONE contiguous float buffer laid out row-major, so scanning a
//  channel is a strided walk over sequential memory exactly like the previous
//  fixed struct. The column set is decided at load time, which is what makes
//  old and new recordings readable by the same code.
//
//  Timestamps are kept in a parallel double buffer: a float loses the third
//  decimal once the ECU uptime counter grows large.
//==============================================================================

class TelemetryDataset {
public:
    // Declares the columns. Must be called before any row is added.
    void setColumns(const std::vector<ColumnDef>& defs) {
        m_defs = defs;
        m_lookup.clear();
        for (size_t i = 0; i < m_defs.size(); ++i) m_lookup[m_defs[i].name] = static_cast<int>(i);
        m_present.assign(m_defs.size(), 0);
        m_values.clear();
        m_timestamps.clear();
        m_rowCount = 0;
        m_timestampIndex = indexOf("timestamp");
    }

    // Appends a column not covered by the current schema (a channel from an
    // older or newer recording). Only valid before rows are added.
    int addColumn(const ColumnDef& def) {
        const int existing = indexOf(def.name);
        if (existing >= 0) return existing;
        m_defs.push_back(def);
        m_present.push_back(0);
        const int index = static_cast<int>(m_defs.size()) - 1;
        m_lookup[def.name] = index;
        return index;
    }

    void reserveRows(size_t rows) {
        m_values.reserve(rows * m_defs.size());
        m_timestamps.reserve(rows);
    }

    // Adds a zeroed row and returns its index
    size_t pushRow() {
        m_values.resize(m_values.size() + m_defs.size(), 0.0f);
        m_timestamps.push_back(0.0);
        return m_rowCount++;
    }

    // Direct access to a row's storage. Loading loops use this to avoid paying
    // for an index multiplication and two branches on every single field.
    // The timestamp slot is unused here: it lives in the parallel double buffer.
    inline float* rowData(size_t row) { return m_values.data() + row * m_defs.size(); }

    inline void setValue(size_t row, int column, double value) {
        if (column < 0) return;
        if (column == m_timestampIndex) m_timestamps[row] = value;
        else m_values[row * m_defs.size() + static_cast<size_t>(column)] = static_cast<float>(value);
    }

    inline double value(size_t row, int column) const {
        if (column < 0) return 0.0;
        if (column == m_timestampIndex) return m_timestamps[row];
        return static_cast<double>(m_values[row * m_defs.size() + static_cast<size_t>(column)]);
    }

    inline double timestamp(size_t row) const { return m_timestamps[row]; }
    inline void setTimestamp(size_t row, double value) { m_timestamps[row] = value; }

    int indexOf(const std::string& name) const {
        const auto it = m_lookup.find(name);
        return it == m_lookup.end() ? -1 : it->second;
    }

    void markPresent(int column) { if (column >= 0) m_present[static_cast<size_t>(column)] = 1; }
    bool isPresent(int column) const { return column >= 0 && m_present[static_cast<size_t>(column)] != 0; }

    size_t rowCount() const { return m_rowCount; }
    size_t columnCount() const { return m_defs.size(); }
    const std::vector<ColumnDef>& definitions() const { return m_defs; }
    const ColumnDef& definition(int column) const { return m_defs[static_cast<size_t>(column)]; }

    void clear() {
        m_values.clear();
        m_timestamps.clear();
        m_rowCount = 0;
    }

    bool empty() const { return m_rowCount == 0; }

    // Builds the string map the UI consumes. Columns that were absent from the
    // source are left out entirely, so widgets naturally show nothing for them.
    DbRow toDbRow(size_t row) const {
        DbRow out;
        for (size_t i = 0; i < m_defs.size(); ++i) {
            if (!m_present[i]) continue;
            const ColumnDef& def = m_defs[i];
            out[def.name] = TelemetrySchema::formatValue(def.format, def.decimals, value(row, static_cast<int>(i)));
        }
        TelemetrySchema::expandDerivedKeys(out);
        return out;
    }

    // Keeps one row out of `step`, preserving columns and presence flags
    TelemetryDataset decimated(size_t step) const {
        TelemetryDataset out;
        out.m_defs = m_defs;
        out.m_lookup = m_lookup;
        out.m_present = m_present;
        out.m_timestampIndex = m_timestampIndex;
        if (step == 0) step = 1;

        const size_t stride = m_defs.size();
        const size_t rows = (m_rowCount + step - 1) / step;
        out.m_values.reserve(rows * stride);
        out.m_timestamps.reserve(rows);

        for (size_t r = 0; r < m_rowCount; r += step) {
            out.m_values.insert(out.m_values.end(),
                                m_values.begin() + r * stride,
                                m_values.begin() + (r + 1) * stride);
            out.m_timestamps.push_back(m_timestamps[r]);
            ++out.m_rowCount;
        }
        return out;
    }

private:
    std::vector<ColumnDef> m_defs;
    std::unordered_map<std::string, int> m_lookup;
    std::vector<uint8_t> m_present;
    std::vector<float> m_values;
    std::vector<double> m_timestamps;
    size_t m_rowCount = 0;
    int m_timestampIndex = -1;
};
