/**
 * @file PlaybackManager.hpp
 * @brief Management of the session replayed from file: time navigation, plot data, CSV export.
 *
 * @details
 * Keeps two copies of the dataset:
 *  - **high-res** (200 Hz, every row): used for the CSV export;
 *  - **low-res** (10 Hz, one row every 20): used by the UI, so plots stay
 *    smooth at 60 FPS even with long sessions.
 *
 * In playback mode the windows do NOT receive callbacks: every frame they read
 * the current row with getCurrentRow() and, for plots, the whole series with
 * getPlotTimeline()/getPlotData(). The current index (of the low-res copy) is
 * moved by the PlaybackControlsWindow.
 *
 * All public methods are thread-safe (internal mutex).
 */
#pragma once
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <atomic>
#include <thread>
#include "Telemetry/file_reading/TelemetryDataset.hpp"

/**
 * @brief Session loaded from file and playback cursor.
 */
class PlaybackManager {
public:
    PlaybackManager() = default;
    /** @brief Cancels and waits for any running export. */
    ~PlaybackManager();

    /**
     * @brief Takes ownership of the loaded dataset and prepares the decimated copy for the UI.
     * @param data            Dataset (moved).
     * @param originalFileExt Lowercase extension of the source file (".bin" or ".csv"),
     *                        used to enable the export.
     */
    void setDataset(TelemetryDataset&& data, const std::string& originalFileExt);
    /** @brief Clears both copies and resets the cursor. */
    void clearData();

    /**
     * @brief Moves the cursor (index in the 10 Hz low-res copy).
     * @param index New index (ignored if out of range).
     */
    void setCurrentIndex(size_t index);
    /** @brief Current index in the low-res copy. */
    size_t getCurrentIndex() const;
    /** @brief Number of rows of the low-res copy (slider upper bound). */
    size_t getTotalRows() const;

    /**
     * @brief Current row as a DbRow, generated on the fly (same format as live).
     * @return The row, or std::nullopt if there is no data.
     */
    std::optional<DbRow> getCurrentRow() const;
    /**
     * @brief Row at any index of the low-res copy.
     * @param index Row index.
     * @return The row, or std::nullopt if out of range.
     */
    std::optional<DbRow> getRowAtIndex(size_t index) const;

    /**
     * @brief Time axis (s) of the whole low-res session, for ImPlot.
     * @return One timestamp per row.
     */
    std::vector<double> getPlotTimeline() const;
    /**
     * @brief Full series of a channel (low-res copy), for ImPlot.
     * @param key Column name.
     * @return One value per row (all 0 if the column does not exist).
     */
    std::vector<double> getPlotData(const std::string& key) const;

    /** @brief true if export is possible: .bin source with data loaded. */
    bool canGenerateCsv() const;
    /**
     * @brief Exports the whole session at high resolution to CSV, in the background.
     *
     * Only the columns present in the recording are exported, formatted with
     * the same formatter as the logger and the UI: re-importing the file gives
     * back exactly the same values.
     *
     * @param outputPath Path of the CSV file to create.
     */
    void generateCsvFromBin(const std::string& outputPath);
    /** @brief true while the export thread is running. */
    bool isExportingCsv() const;
    /** @brief Requests the export to stop and waits for its thread to finish. */
    void cancelExport();

private:
    TelemetryDataset m_highResData; ///< 200 Hz: accurate calculations and CSV export.
    TelemetryDataset m_lowResData;  ///< 10 Hz: smooth plots and navigation in the UI.

    std::string m_originalFileExt;  ///< Extension of the source file (".bin" / ".csv").
    size_t m_currentIndex = 0;      ///< Playback cursor (low-res index).
    mutable std::mutex m_dataMutex; ///< Protects datasets and cursor.

    std::atomic<bool> m_isExporting{false};   ///< Export in progress.
    std::atomic<bool> m_cancelExport{false};  ///< Export stop request.
    std::thread m_exportThread;               ///< Export thread.
};
