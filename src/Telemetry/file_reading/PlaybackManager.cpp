#include "Telemetry/file_reading/PlaybackManager.hpp"
#include <fstream>
#include <iostream>

// One UI sample every 20 stored rows: 200 Hz recordings become a 10 Hz plotting
// buffer, which keeps ImPlot fluid without changing what the data means
static constexpr size_t UI_DECIMATION = 20;

void PlaybackManager::setDataset(TelemetryDataset&& data, const std::string& originalFileExt) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_highResData = std::move(data);
    m_originalFileExt = originalFileExt;
    m_currentIndex = 0;
    m_lowResData = m_highResData.decimated(UI_DECIMATION);
}

void PlaybackManager::clearData() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_highResData.clear();
    m_lowResData.clear();
    m_currentIndex = 0;
}

void PlaybackManager::setCurrentIndex(size_t index) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (index < m_lowResData.rowCount()) {
        m_currentIndex = index;
    }
}

size_t PlaybackManager::getCurrentIndex() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_currentIndex;
}

size_t PlaybackManager::getTotalRows() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_lowResData.rowCount();
}

std::optional<DbRow> PlaybackManager::getCurrentRow() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_currentIndex < m_lowResData.rowCount()) {
        return m_lowResData.toDbRow(m_currentIndex);
    }
    return std::nullopt;
}

std::optional<DbRow> PlaybackManager::getRowAtIndex(size_t index) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (index < m_lowResData.rowCount()) {
        return m_lowResData.toDbRow(index);
    }
    return std::nullopt;
}

std::vector<double> PlaybackManager::getPlotTimeline() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<double> timeline(m_lowResData.rowCount());
    for (size_t i = 0; i < m_lowResData.rowCount(); ++i) {
        timeline[i] = m_lowResData.timestamp(i);
    }
    return timeline;
}

std::vector<double> PlaybackManager::getPlotData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    std::vector<double> data(m_lowResData.rowCount(), 0.0);

    const int column = m_lowResData.indexOf(key);
    if (column < 0) return data;

    for (size_t i = 0; i < m_lowResData.rowCount(); ++i) {
        data[i] = m_lowResData.value(i, column);
    }
    return data;
}

bool PlaybackManager::canGenerateCsv() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_originalFileExt == ".bin" && !m_highResData.empty();
}

PlaybackManager::~PlaybackManager() {
    cancelExport();
}

bool PlaybackManager::isExportingCsv() const {
    return m_isExporting.load();
}

void PlaybackManager::cancelExport() {
    if (m_isExporting.load()) {
        m_cancelExport.store(true);
    }
    if (m_exportThread.joinable()) {
        m_exportThread.join();
    }
}

//==============================================================================
//  High-resolution export.
//
//  Header and values are generated from the dataset itself, using the very same
//  formatter the live logger and the UI use. Re-importing the produced file
//  therefore yields exactly the values shown before exporting.
//==============================================================================

void PlaybackManager::generateCsvFromBin(const std::string& outputPath) {
    if (m_isExporting.load()) return;

    // Cleanly join any previously finished thread before spawning a new one
    if (m_exportThread.joinable()) {
        m_exportThread.join();
    }

    m_isExporting.store(true);
    m_cancelExport.store(false);

    m_exportThread = std::thread([this, outputPath]() {
        std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "ERROR [PlaybackManager]: Could not open file for CSV export." << std::endl;
            m_isExporting.store(false);
            return;
        }

        TelemetryDataset localData;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            localData = m_highResData;
        }

        // Only channels the recording actually carried are exported
        std::vector<int> columns;
        for (size_t i = 0; i < localData.columnCount(); ++i) {
            if (localData.isPresent(static_cast<int>(i))) columns.push_back(static_cast<int>(i));
        }

        for (size_t i = 0; i < columns.size(); ++i) {
            out << localData.definition(columns[i]).name;
            out << (i + 1 == columns.size() ? '\n' : ';');
        }

        // One reused line buffer: no per-value allocation and a single write
        // per row, which keeps the export as cheap as the old hardcoded writer
        std::string line;
        line.reserve(columns.size() * 12);
        char cell[64];

        for (size_t row = 0; row < localData.rowCount(); ++row) {
            if (m_cancelExport.load()) break;

            line.clear();
            for (size_t i = 0; i < columns.size(); ++i) {
                const ColumnDef& def = localData.definition(columns[i]);
                const size_t length = TelemetrySchema::formatValueInto(
                    def.format, def.decimals, localData.value(row, columns[i]), cell, sizeof(cell));
                line.append(cell, length);
                line.push_back(i + 1 == columns.size() ? '\n' : ';');
            }
            out.write(line.data(), static_cast<std::streamsize>(line.size()));
        }

        out.close();
        if (!m_cancelExport.load()) {
            std::cout << "INFO [PlaybackManager]: High-Res CSV exported successfully to " << outputPath << std::endl;
        }

        m_isExporting.store(false);
    });
}
