#pragma once
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <atomic>
#include <thread>
#include "Telemetry/file_reading/TelemetryDataset.hpp"

class PlaybackManager {
public:
    PlaybackManager() = default;
    ~PlaybackManager();

    // Takes ownership of a loaded session and prepares the decimated UI copy
    void setDataset(TelemetryDataset&& data, const std::string& originalFileExt);
    void clearData();

    // Timeline navigation (operates on the 10Hz low-res buffer for UI performance)
    void setCurrentIndex(size_t index);
    size_t getCurrentIndex() const;
    size_t getTotalRows() const;

    // Generates a traditional DbRow Just-In-Time (JIT) to ensure backward compatibility with UI components
    std::optional<DbRow> getCurrentRow() const;
    std::optional<DbRow> getRowAtIndex(size_t index) const;

    // Ultra-fast data extraction for ImPlot (contiguous strided reads)
    std::vector<double> getPlotTimeline() const;
    std::vector<double> getPlotData(const std::string& key) const;

    // High-Res CSV Export capabilities
    bool canGenerateCsv() const;
    void generateCsvFromBin(const std::string& outputPath);
    bool isExportingCsv() const;
    void cancelExport();

private:
    TelemetryDataset m_highResData; // 200 Hz for accurate calculations and CSV export
    TelemetryDataset m_lowResData;  // 10 Hz for smooth 60FPS UI Plotting

    std::string m_originalFileExt;
    size_t m_currentIndex = 0;
    mutable std::mutex m_dataMutex;

    std::atomic<bool> m_isExporting{false};
    std::atomic<bool> m_cancelExport{false};
    std::thread m_exportThread;
};
