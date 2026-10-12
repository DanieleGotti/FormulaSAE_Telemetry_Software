/**
 * @file TelemetryDataset.hpp
 * @brief Column-agnostic in-memory container of a session loaded from file.
 *
 * @details
 * Values live in ONE contiguous `float` buffer laid out row-major: cell
 * (row r, column c) is `m_values[r * columnCount + c]`. Scanning a channel is
 * therefore a constant-stride walk over sequential memory, very efficient for
 * tens of thousands of rows.
 *
 * The column set is decided at load time: this is what lets the same code read
 * old and new recordings. Every column has a "present" flag: columns the
 * recording did not carry stay not present and are neither shown nor exported.
 *
 * Timestamps live in a parallel `double` buffer: a float loses the third
 * decimal once the ECU uptime counter grows large.
 */
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"

/**
 * @brief Table of numeric values (rows x columns) with columns defined at runtime.
 */
class TelemetryDataset {
public:
    /**
     * @brief Declares the columns and empties the dataset. Must be called before adding rows.
     * @param defs Column definitions (typically TelemetrySchema::columns()).
     */
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

    /**
     * @brief Appends a column not covered by the current schema (a channel from
     *        an older or newer recording).
     * @warning Only valid BEFORE rows are added (the buffer is not resized).
     * @param def Column definition.
     * @return Column index (the existing one if the name was already there).
     */
    int addColumn(const ColumnDef& def) {
        const int existing = indexOf(def.name);
        if (existing >= 0) return existing;
        m_defs.push_back(def);
        m_present.push_back(0);
        const int index = static_cast<int>(m_defs.size()) - 1;
        m_lookup[def.name] = index;
        return index;
    }

    /**
     * @brief Preallocates memory for an estimated number of rows.
     * @param rows Expected rows.
     */
    void reserveRows(size_t rows) {
        m_values.reserve(rows * m_defs.size());
        m_timestamps.reserve(rows);
    }

    /**
     * @brief Adds a row with all values set to zero.
     * @return Index of the new row.
     */
    size_t pushRow() {
        m_values.resize(m_values.size() + m_defs.size(), 0.0f);
        m_timestamps.push_back(0.0);
        return m_rowCount++;
    }

    /**
     * @brief Direct access to the cells of a row (loaders' fast path).
     *
     * Avoids an index multiplication and two branches for every single field.
     * The cell of the `timestamp` column is NOT used here: the timestamp lives
     * in the parallel double buffer (use setTimestamp()).
     *
     * @param row Row index.
     * @return Pointer to the first cell of the row.
     */
    inline float* rowData(size_t row) { return m_values.data() + row * m_defs.size(); }

    /**
     * @brief Writes a value (the timestamp column is handled automatically).
     * @param row    Row index.
     * @param column Column index (does nothing if < 0).
     * @param value  Value.
     */
    inline void setValue(size_t row, int column, double value) {
        if (column < 0) return;
        if (column == m_timestampIndex) m_timestamps[row] = value;
        else m_values[row * m_defs.size() + static_cast<size_t>(column)] = static_cast<float>(value);
    }

    /**
     * @brief Reads a value (the timestamp column is handled automatically).
     * @param row    Row index.
     * @param column Column index.
     * @return Value, or 0 if the column is < 0.
     */
    inline double value(size_t row, int column) const {
        if (column < 0) return 0.0;
        if (column == m_timestampIndex) return m_timestamps[row];
        return static_cast<double>(m_values[row * m_defs.size() + static_cast<size_t>(column)]);
    }

    /** @brief Timestamp (s) of a row. */
    inline double timestamp(size_t row) const { return m_timestamps[row]; }
    /** @brief Sets the timestamp (s) of a row. */
    inline void setTimestamp(size_t row, double value) { m_timestamps[row] = value; }

    /**
     * @brief Index of a column given its name.
     * @param name Column name.
     * @return Index, or -1 if it does not exist.
     */
    int indexOf(const std::string& name) const {
        const auto it = m_lookup.find(name);
        return it == m_lookup.end() ? -1 : it->second;
    }

    /** @brief Marks a column as present in the source (ignored if column < 0). */
    void markPresent(int column) { if (column >= 0) m_present[static_cast<size_t>(column)] = 1; }
    /** @brief true if the column exists and is present in the source. */
    bool isPresent(int column) const { return column >= 0 && m_present[static_cast<size_t>(column)] != 0; }

    /** @brief Number of rows. */
    size_t rowCount() const { return m_rowCount; }
    /** @brief Number of columns (present or not). */
    size_t columnCount() const { return m_defs.size(); }
    /** @brief Definitions of all columns. */
    const std::vector<ColumnDef>& definitions() const { return m_defs; }
    /** @brief Definition of a column (format and decimals used for printing). */
    const ColumnDef& definition(int column) const { return m_defs[static_cast<size_t>(column)]; }

    /** @brief Removes all rows, keeping the columns. */
    void clear() {
        m_values.clear();
        m_timestamps.clear();
        m_rowCount = 0;
    }

    /** @brief true if there are no rows. */
    bool empty() const { return m_rowCount == 0; }

    /**
     * @brief Builds the text DbRow the UI consumes for one row.
     *
     * Columns absent from the source are left out entirely, so widgets
     * naturally show nothing for them. Derived keys (e.g. OnlineModuleN) are added.
     *
     * @param row Row index.
     * @return Row formatted with TelemetrySchema::formatValue().
     */
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

    /**
     * @brief Creates a decimated copy (one row every `step`), with the same columns and flags.
     * @param step Decimation step (0 is treated as 1).
     * @return New dataset with about rowCount()/step rows.
     */
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
    std::vector<ColumnDef> m_defs;                  ///< Column definitions.
    std::unordered_map<std::string, int> m_lookup;  ///< Name -> column index.
    std::vector<uint8_t> m_present;                 ///< Presence flag per column (1 = present in the source).
    std::vector<float> m_values;                    ///< Values, row-major (rowCount x columnCount).
    std::vector<double> m_timestamps;               ///< Timestamps in seconds, one per row.
    size_t m_rowCount = 0;                          ///< Number of rows.
    int m_timestampIndex = -1;                      ///< Index of the "timestamp" column (-1 if absent).
};
