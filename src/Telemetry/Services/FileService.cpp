/**
 * @file FileService.cpp
 * @brief Implementation of .bin/.csv file loading (see FileService.hpp).
 *
 * @details
 * The code is optimised for large files (tens of MB): the file is read into
 * memory with a single read, the CSV is tokenized in place without creating
 * strings and values go straight into the contiguous TelemetryDataset buffer.
 * The decoding rules (and, as far as possible, the statistics rules) MUST stay
 * aligned with PacketParser and DataAggregator, otherwise a replayed file would
 * show different values from the ones seen live.
 */
#include "Telemetry/Services/FileService.hpp"
#include "Telemetry/data_acquisition/TelemetrySchema.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_map>

FileService::FileService() = default;

FileService::~FileService() {
    // Safely stop background reading if the app is closed, avoiding orphan threads and crashes
    m_cancelRequested.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool FileService::configure(const FileConfig& config) {
    if (m_isLoading) return false;
    m_config = config;
    return true;
}

void FileService::startLoading() {
    if (m_isLoading || m_isFinished) return;
    m_isLoading = true;
    m_cancelRequested.store(false); // reset the cancellation request for the new load
    m_thread = std::thread(&FileService::loadingLoop, this);
}

bool FileService::isLoading() const { return m_isLoading; }
bool FileService::isFinished() const { return m_isFinished; }

TelemetryDataset FileService::getResult() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return std::move(m_loadedData);
}

void FileService::loadingLoop() {
    // Normalise path separators and get the lowercase extension
    std::string filePath = m_config.filePath;
    std::replace(filePath.begin(), filePath.end(), '\\', '/');

    const size_t dot = filePath.find_last_of(".");
    std::string ext = (dot == std::string::npos) ? "" : filePath.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".csv") {
        loadCsv(filePath);
    } else if (ext == ".bin") {
        loadBin(filePath);
    } else {
        std::cerr << "ERROR [FileService]: File format not supported." << std::endl;
        m_isLoading = false;
        m_isFinished = true;
    }
}

//==============================================================================
//  Column setup
//==============================================================================

void FileService::prepareColumns(TelemetryDataset& dataset) {
    dataset.setColumns(TelemetrySchema::columns());

    // Channels that only exist in retired layouts still need a column
    for (const auto* layouts : { &TelemetrySchema::layoutsA(), &TelemetrySchema::layoutsB() }) {
        for (const auto& layout : *layouts) {
            for (const auto& field : layout.fields) {
                dataset.addColumn({ field.name, field.format, field.decimals });
            }
        }
    }
}

//==============================================================================
//  Statistics
//==============================================================================

void FileService::computeMissingStats(TelemetryDataset& dataset) {
    const int WINDOW = 600; // 3 s at 200 Hz, the same cadence as the live aggregator
    const size_t rows = dataset.rowCount();
    if (rows == 0) return;

    // ---- Windowed mean / variance / standard deviation ----
    struct VarianceTarget { int source; int mean; int var; int std; };
    std::vector<VarianceTarget> varianceTargets;
    for (const auto& sensor : TelemetrySchema::varianceChannels()) {
        const int mean = dataset.indexOf(sensor + "_mean");
        const int var  = dataset.indexOf(sensor + "_var");
        const int std  = dataset.indexOf(sensor + "_std");
        // Skip channels the source already provided
        if (dataset.isPresent(mean) && dataset.isPresent(var) && dataset.isPresent(std)) continue;
        // ...and channels this recording never carried, so they stay blank
        // instead of showing a fabricated 0.000
        const int source = dataset.indexOf(sensor);
        if (!dataset.isPresent(source)) continue;
        varianceTargets.push_back({ source, mean, var, std });
    }

    // ---- Session-cumulative max / mean ----
    struct MaxMeanTarget { int sourceA; int sourceB; int max; int mean; double runMax; double runSum; };
    std::vector<MaxMeanTarget> maxMeanTargets;
    for (const auto& channel : TelemetrySchema::maxMeanChannels()) {
        const int max  = dataset.indexOf(channel + "_max");
        const int mean = dataset.indexOf(channel + "_mean");
        if (dataset.isPresent(max) && dataset.isPresent(mean)) continue;

        MaxMeanTarget target{};
        target.max = max;
        target.mean = mean;
        target.runMax = std::numeric_limits<double>::lowest();
        target.runSum = 0.0;
        if (channel == "speed") {
            // Vehicle speed is the average of the two rear wheels
            target.sourceA = dataset.indexOf("rear_left_velocity");
            target.sourceB = dataset.indexOf("rear_right_velocity");
            if (!dataset.isPresent(target.sourceA) || !dataset.isPresent(target.sourceB)) continue;
        } else {
            target.sourceA = dataset.indexOf(channel);
            target.sourceB = -1;
            if (!dataset.isPresent(target.sourceA)) continue;
        }
        maxMeanTargets.push_back(target);
    }

    // ---- Distance travelled, integrated from the speedometer ----
    // Skipped when the source already carries it, so a bin -> csv -> bin round
    // trip reproduces the very same numbers instead of re-deriving them
    const int idxMeters   = dataset.indexOf("meters");
    const int idxRearLeft = dataset.indexOf("rear_left_velocity");
    const int idxRearRight= dataset.indexOf("rear_right_velocity");
    const bool computeMeters = !dataset.isPresent(idxMeters)
                            && dataset.isPresent(idxRearLeft)
                            && dataset.isPresent(idxRearRight);

    if (computeMeters) {
        // The live aggregator integrates the velocities as they appear in the
        // dataset, that is rounded to the precision the schema declares for
        // them. Quantising here too makes a replayed file report exactly the
        // same distance the driver saw on track.
        const double quantum = std::pow(10.0, dataset.definition(idxRearLeft).decimals);
        // nearbyint honours the active FP rounding mode, the same one printf
        // uses when the live path renders the value, so ties land the same way
        auto canonical = [quantum](double v) { return std::nearbyint(v * quantum) / quantum; };

        double distance = 0.0;
        double previousTs = -1.0;
        for (size_t r = 0; r < rows; ++r) {
            const double timestamp = dataset.timestamp(r);
            const double speed_kmh = (canonical(dataset.value(r, idxRearLeft))
                                    + canonical(dataset.value(r, idxRearRight))) * 0.5;

            // Same rule as the live aggregator: a long gap means lost samples
            if (previousTs >= 0.0) {
                const double dt = timestamp - previousTs;
                if (dt > 0.0 && dt <= TelemetrySchema::MAX_DISTANCE_GAP_S) {
                    distance += speed_kmh * TelemetrySchema::KMH_TO_MS * dt;
                }
            }
            previousTs = timestamp;
            dataset.setValue(r, idxMeters, distance);
        }
        dataset.markPresent(idxMeters);
    }

    if (varianceTargets.empty() && maxMeanTargets.empty()) return;

    size_t sessionCount = 0;

    // Scan in blocks of WINDOW rows ("tumbling" windows, like the live path)
    for (size_t start = 0; start < rows; start += WINDOW) {
        if (m_cancelRequested.load()) return;

        const size_t end = std::min(start + static_cast<size_t>(WINDOW), rows);
        const size_t count = end - start;

        // The variance of each block is computed in a single pass (sum and sum of squares).
        // Note: this is the population variance (divisor n), live uses n-1
        // (DataAggregator::calculateStats): with 600 samples the difference is negligible
        for (auto& target : varianceTargets) {
            double sum = 0.0, sumSquares = 0.0;
            for (size_t r = start; r < end; ++r) {
                const double v = dataset.value(r, target.source);
                sum += v;
                sumSquares += v * v;
            }
            const double mean = sum / count;
            double variance = (sumSquares / count) - (mean * mean);
            if (variance < 0.0) variance = 0.0;
            const double stdDev = std::sqrt(variance);

            for (size_t r = start; r < end; ++r) {
                dataset.setValue(r, target.mean, mean);
                dataset.setValue(r, target.var, variance);
                dataset.setValue(r, target.std, stdDev);
            }
        }

        // Accumulate max and sum up to and including this block
        for (auto& target : maxMeanTargets) {
            for (size_t r = start; r < end; ++r) {
                double v = dataset.value(r, target.sourceA);
                if (target.sourceB >= 0) v = (v + dataset.value(r, target.sourceB)) * 0.5;
                if (v > target.runMax) target.runMax = v;
                target.runSum += v;
            }
        }
        sessionCount += count;

        // Write the updated cumulative values into every row of the block
        for (const auto& target : maxMeanTargets) {
            const double mean = target.runSum / sessionCount;
            for (size_t r = start; r < end; ++r) {
                dataset.setValue(r, target.max, target.runMax);
                dataset.setValue(r, target.mean, mean);
            }
        }
    }

    for (const auto& target : varianceTargets) {
        dataset.markPresent(target.mean);
        dataset.markPresent(target.var);
        dataset.markPresent(target.std);
    }
    for (const auto& target : maxMeanTargets) {
        dataset.markPresent(target.max);
        dataset.markPresent(target.mean);
    }
}

//==============================================================================
//  Binary loading
//==============================================================================

void FileService::loadBin(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "ERROR [FileService]: Unable to open BIN file." << std::endl;
        m_isLoading = false; m_isFinished = true; return;
    }

    // Read the whole file into memory with a single operation
    const size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    TelemetryDataset dataset;
    prepareColumns(dataset);

    const auto& layoutsA = TelemetrySchema::layoutsA();
    const auto& layoutsB = TelemetrySchema::layoutsB();

    // Column indices resolved once per layout, so the hot loop only does memcpy + scaling
    std::unordered_map<const PacketLayout*, std::vector<int>> columnCache;
    auto columnsFor = [&](const PacketLayout& layout) -> const std::vector<int>& {
        auto it = columnCache.find(&layout);
        if (it != columnCache.end()) return it->second;
        std::vector<int> indices;
        indices.reserve(layout.fields.size());
        for (const auto& field : layout.fields) indices.push_back(dataset.indexOf(field.name));
        return columnCache.emplace(&layout, std::move(indices)).first->second;
    };

    const int idxLostA   = dataset.indexOf("lost_packets_A");
    const int idxLostB   = dataset.indexOf("lost_packets_B");
    const int idxLostTot = dataset.indexOf("lost_packets_tot");

    // Row count estimate: file size / size of one A packet
    dataset.reserveRows(fileSize / (layoutsA.front().payloadSize ? layoutsA.front().payloadSize : 1));

    // Latest battery values (column, value), replayed into every fast row
    std::vector<std::pair<int, double>> lastB;
    bool hasB = false;

    // Lost packet counting state (same rules as PacketParser)
    uint32_t lastTimestampA = 0, lastTimestampB = 0;
    uint32_t lostA = 0, lostB = 0;
    bool firstA = true, firstB = true;

    // Locked layouts and first layout actually used (for the log and the presence flags)
    int lockedA = 0, lockedB = 0;
    const PacketLayout* usedA = nullptr;
    const PacketLayout* usedB = nullptr;

    // Same strategy as PacketParser::matchLayout: locked layout first, then the others
    auto tryMatch = [](const std::vector<PacketLayout>& layouts, int& locked,
                       const uint8_t* data, size_t available) -> const PacketLayout* {
        if (locked >= 0 && locked < static_cast<int>(layouts.size()) &&
            TelemetrySchema::matches(layouts[locked], data, available)) {
            return &layouts[locked];
        }
        for (size_t i = 0; i < layouts.size(); ++i) {
            if (static_cast<int>(i) == locked) continue;
            if (TelemetrySchema::matches(layouts[i], data, available)) {
                locked = static_cast<int>(i);
                return &layouts[i];
            }
        }
        return nullptr;
    };

    size_t cursor = 0;   // current position in the buffer
    size_t skipped = 0;  // bytes skipped because they do not belong to valid packets
    while (cursor + 2 <= buffer.size()) {
        if (m_cancelRequested.load()) {
            std::cout << "WARNING [FileService]: BIN loading aborted by user." << std::endl;
            break;
        }

        const uint8_t* at = buffer.data() + cursor;
        const size_t available = buffer.size() - cursor;

        // Decodes a battery packet into the lastB cache (replayed into the following rows)
        auto decodeB = [&](const uint8_t* data, size_t room) -> const PacketLayout* {
            const PacketLayout* layout = tryMatch(layoutsB, lockedB, data, room);
            if (!layout) return nullptr;
            if (!usedB) usedB = layout;

            uint32_t ts = 0; std::memcpy(&ts, data + 2, 4);
            if (!firstB && ts > lastTimestampB) {
                const uint32_t diff = ts - lastTimestampB;
                if (diff > 40 && diff < 100000) lostB += (diff / 40) - 1;
            }
            lastTimestampB = ts;
            firstB = false;

            const std::vector<int>& indices = columnsFor(*layout);
            lastB.clear();
            lastB.reserve(layout->fields.size());
            for (size_t i = 0; i < layout->fields.size(); ++i) {
                lastB.emplace_back(indices[i],
                                   TelemetrySchema::readField(data, layout->fieldOffset(i), layout->fields[i]));
            }
            hasB = true;
            return layout;
        };

        if (at[0] == 0xCC && at[1] == 0x11) {
            const PacketLayout* layout = tryMatch(layoutsA, lockedA, at, available);
            if (layout) {
                if (!usedA) { usedA = layout; }
                size_t consumed = layout->payloadSize;

                // Same look-ahead the live parser performs: when a battery packet
                // follows immediately, merge it into THIS row instead of the next
                // one, so a file replays exactly what was seen live
                if (available >= consumed + 2 && at[consumed] == 0xEE && at[consumed + 1] == 0x11) {
                    if (const PacketLayout* layoutB = decodeB(at + consumed, available - consumed)) {
                        consumed += layoutB->payloadSize;
                    }
                }

                uint32_t ts = 0; std::memcpy(&ts, at + 2, 4);
                if (!firstA && ts > lastTimestampA) {
                    const uint32_t diff = ts - lastTimestampA;
                    if (diff > 1 && diff < 5000) lostA += (diff - 1);
                }
                lastTimestampA = ts;
                firstA = false;

                const size_t row = dataset.pushRow();
                dataset.setTimestamp(row, ts * 0.005); // 5 ms ticks -> seconds

                // Write straight into the row cells (fast path)
                float* cells = dataset.rowData(row);
                const std::vector<int>& indices = columnsFor(*layout);
                for (size_t i = 0; i < layout->fields.size(); ++i) {
                    const int column = indices[i];
                    if (column < 0) continue;
                    cells[column] = static_cast<float>(
                        TelemetrySchema::readField(at, layout->fieldOffset(i), layout->fields[i]));
                }
                for (const auto& entry : lastB) {
                    if (entry.first >= 0) cells[entry.first] = static_cast<float>(entry.second);
                }

                if (idxLostA >= 0)   cells[idxLostA]   = static_cast<float>(lostA);
                if (idxLostB >= 0)   cells[idxLostB]   = static_cast<float>(lostB);
                if (idxLostTot >= 0) cells[idxLostTot] = static_cast<float>(lostA + lostB);

                cursor += consumed;
                continue;
            }
        } else if (at[0] == 0xEE && at[1] == 0x11) {
            if (const PacketLayout* layout = decodeB(at, available)) {
                cursor += layout->payloadSize;
                continue;
            }
        }
        // No valid packet at this position: move forward one byte and try again
        ++cursor;
        ++skipped;
    }

    // Flag which channels this recording actually carried
    dataset.markPresent(dataset.indexOf("timestamp"));
    dataset.markPresent(idxLostA);
    dataset.markPresent(idxLostB);
    dataset.markPresent(idxLostTot);
    if (usedA) for (const auto& field : usedA->fields) dataset.markPresent(dataset.indexOf(field.name));
    if (usedB && hasB) for (const auto& field : usedB->fields) dataset.markPresent(dataset.indexOf(field.name));

    std::cout << "INFO [FileService]: BIN decoded with layout "
              << (usedA ? usedA->versionTag : "none") << " / "
              << (usedB ? usedB->versionTag : "none")
              << " - " << dataset.rowCount() << " rows";
    if (skipped) std::cout << ", " << skipped << " stray bytes skipped";
    std::cout << "." << std::endl;

    computeMissingStats(dataset);

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_loadedData = std::move(dataset);
    m_isFinished = true;
    m_isLoading = false;
}

//==============================================================================
//  CSV loading, matched by column name so any past export stays readable
//==============================================================================

void FileService::loadCsv(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "ERROR [FileService]: Unable to open CSV file." << std::endl;
        m_isLoading = false; m_isFinished = true; return;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // '\0' terminated buffer so atof can be used directly on the tokens
    std::vector<char> buffer(fileSize + 1);
    file.read(buffer.data(), fileSize);
    buffer[fileSize] = '\0';

    TelemetryDataset dataset;
    prepareColumns(dataset);

    char* p = buffer.data();
    char* const end = p + fileSize;

    // ---- Header: map every column of the file onto a dataset column ----
    std::vector<std::string> headerNames;
    {
        char* lineStart = p;
        while (p < end && *p != '\n' && *p != '\r') ++p;
        std::string header(lineStart, static_cast<size_t>(p - lineStart));
        while (p < end && (*p == '\n' || *p == '\r')) ++p;

        size_t from = 0;
        while (from <= header.size()) {
            const size_t sep = header.find(';', from);
            const size_t upto = (sep == std::string::npos) ? header.size() : sep;
            std::string name = header.substr(from, upto - from);
            // Trim stray whitespace and a possible UTF-8 BOM on the first column
            while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
            size_t begin = 0;
            while (begin < name.size() && (name[begin] == ' ' || name[begin] == '\t')) ++begin;
            if (headerNames.empty() && name.size() >= begin + 3 &&
                static_cast<unsigned char>(name[begin]) == 0xEF) begin += 3;
            headerNames.push_back(name.substr(begin));

            if (sep == std::string::npos) break;
            from = sep + 1;
        }
    }

    if (headerNames.size() < 2) {
        std::cerr << "ERROR [FileService]: CSV header not recognised." << std::endl;
        m_isLoading = false; m_isFinished = true; return;
    }

    // columnOf[i] = dataset column of the i-th file column
    std::vector<int> columnOf(headerNames.size(), -1);
    int mapped = 0, unknown = 0;
    for (size_t i = 0; i < headerNames.size(); ++i) {
        if (headerNames[i].empty()) continue;
        int index = dataset.indexOf(headerNames[i]);
        if (index < 0) {
            // A channel this build does not know about: keep it rather than drop it
            index = dataset.addColumn({ headerNames[i], ValueFormat::Number, 3 });
            ++unknown;
        }
        columnOf[i] = index;
        dataset.markPresent(index);
        ++mapped;
    }

    // The overwhelming majority of columns are plain numbers: resolving that
    // once per file lets the row loop call atof directly instead of dispatching
    std::vector<ValueFormat> formatOf(headerNames.size(), ValueFormat::Number);
    std::vector<uint8_t> isNumeric(headerNames.size(), 1);
    for (size_t i = 0; i < headerNames.size(); ++i) {
        if (columnOf[i] >= 0) {
            formatOf[i] = dataset.definition(columnOf[i]).format;
            isNumeric[i] = (formatOf[i] == ValueFormat::Number) ? 1 : 0;
        }
    }

    dataset.reserveRows(fileSize / 300); // estimate: ~300 bytes per row
    const int timestampColumn = dataset.indexOf("timestamp");
    const size_t columnCount = headerNames.size();
    std::vector<char*> tokens(columnCount, nullptr);

    while (p < end) {
        if (m_cancelRequested.load()) break;

        size_t tokenIndex = 0;
        tokens[0] = p;

        // Tokenize in place: every ';' becomes '\0', no std::string is built for the values
        while (p < end && *p != '\n' && *p != '\r') {
            if (*p == ';') {
                *p = '\0';
                ++tokenIndex;
                if (tokenIndex < columnCount) tokens[tokenIndex] = p + 1;
            }
            ++p;
        }
        if (p < end) { *p = '\0'; ++p; }

        // A row is usable as long as it carries a timestamp and some data
        if (tokenIndex >= 1) {
            const size_t row = dataset.pushRow();
            float* cells = dataset.rowData(row);
            for (size_t i = 0; i <= tokenIndex && i < columnCount; ++i) {
                const int column = columnOf[i];
                if (column < 0 || !tokens[i]) continue;
                const double value = isNumeric[i] ? std::atof(tokens[i])
                                                  : TelemetrySchema::parseValue(formatOf[i], tokens[i]);
                if (column == timestampColumn) dataset.setTimestamp(row, value);
                else cells[column] = static_cast<float>(value);
            }
        }

        while (p < end && (*p == '\n' || *p == '\r')) ++p;
    }

    std::cout << "INFO [FileService]: CSV decoded - " << dataset.rowCount() << " rows, "
              << mapped << " columns matched";
    if (unknown) std::cout << " (" << unknown << " unknown kept as-is)";
    std::cout << "." << std::endl;

    computeMissingStats(dataset);

    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_loadedData = std::move(dataset);
    m_isFinished = true;
    m_isLoading = false;
}
