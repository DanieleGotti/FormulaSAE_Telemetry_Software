/**
 * @file FileService.hpp
 * @brief Background loading of a recorded session (raw .bin from the SD card or .csv).
 *
 * @details
 * - **.bin**: binary stream identical to the one sent over serial (A and B
 *   packets in sequence). It is decoded with the same layouts and the same logic
 *   as the live parser (B packet look-ahead, lost packet counting).
 * - **.csv**: file with `;` separator and a header (produced by the logger or by
 *   the export). Columns are matched by NAME, so any past CSV stays readable;
 *   unknown columns are kept anyway.
 *
 * In both cases the missing statistics (variance, max/mean, meters) are
 * computed with the same rules as DataAggregator. The result is a
 * TelemetryDataset that the UI hands to PlaybackManager.
 *
 * Typical usage (done by ServiceManager and UiManager):
 * @code
 * fileService->configure({path});
 * fileService->startLoading();          // starts the thread
 * if (fileService->isFinished())        // polled every frame
 *     playback->setDataset(fileService->getResult(), ext);
 * @endcode
 */
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "Telemetry/file_reading/TelemetryDataset.hpp"

/**
 * @brief File loading parameters.
 */
struct FileConfig {
    std::string filePath;  ///< Path of the .bin or .csv file.
};

/**
 * @brief Loads a telemetry file on a dedicated thread.
 */
class FileService {
public:
    FileService();
    /** @brief Requests cancellation and waits for the loading thread to finish. */
    ~FileService();

    /**
     * @brief Sets the file to load.
     * @param config Configuration.
     * @return false if a load is already in progress.
     */
    bool configure(const FileConfig& config);
    /** @brief Starts the loading thread (ignored if already running or already finished). */
    void startLoading();
    /** @brief true while the thread is loading. */
    bool isLoading() const;
    /** @brief true when loading is over (even on error: in that case the dataset is empty). */
    bool isFinished() const;

    /**
     * @brief Returns the loaded dataset, moving it out of the service.
     * @return The dataset (to be called only once, after isFinished()).
     */
    TelemetryDataset getResult();
    /** @brief Current configuration (used by the UI to get the file extension). */
    const FileConfig& getConfig() const { return m_config; }

private:
    /** @brief Thread body: picks the loader from the extension (.csv / .bin). */
    void loadingLoop();
    /**
     * @brief Loads a ';' separated CSV, matching columns by name.
     * @param filePath File path.
     */
    void loadCsv(const std::string& filePath);
    /**
     * @brief Decodes a raw binary file, validating every packet with its CRC.
     * @param filePath File path.
     */
    void loadBin(const std::string& filePath);

    /**
     * @brief Registers in the dataset every column any known layout can produce.
     *
     * This way a recording from an older or newer firmware still finds a valid
     * column for every one of its channels.
     *
     * @param dataset Dataset to prepare.
     */
    static void prepareColumns(TelemetryDataset& dataset);

    /**
     * @brief Computes the statistic columns the source did not already provide.
     *
     * A .bin never carries them; a .csv exported by this software does. In that
     * case the file values are reused, so a bin -> csv -> bin round trip
     * produces identical numbers. Computed statistics:
     *  - mean/variance/std over blocks of 600 rows (3 s at 200 Hz), with the
     *    population variance (divisor n; live uses n-1, see DataAggregator);
     *  - session-cumulative max and mean;
     *  - `meters` distance integrated from the rear wheel speed.
     *
     * @param dataset Dataset to complete (modified in place).
     */
    void computeMissingStats(TelemetryDataset& dataset);

    FileConfig m_config;                          ///< File to load.
    std::atomic<bool> m_isLoading{false};         ///< Loading thread active.
    std::atomic<bool> m_isFinished{false};        ///< Loading completed.
    std::atomic<bool> m_cancelRequested{false};   ///< Cancellation request (checked inside the loops).
    std::thread m_thread;                         ///< Loading thread.

    TelemetryDataset m_loadedData;                ///< Result, handed over by getResult().
    std::mutex m_dataMutex;                       ///< Protects m_loadedData.
};
