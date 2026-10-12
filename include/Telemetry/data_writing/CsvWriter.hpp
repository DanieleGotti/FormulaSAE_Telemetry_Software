/**
 * @file CsvWriter.hpp
 * @brief Recording of the live rows to a CSV file (on-track logging).
 *
 * @details
 * Format of the produced file:
 *  - named `telemetry_YYYYMMDD_hhmmss.csv`, in the `output_data` folder next to the executable;
 *  - `;` separator, first line = header with the column names in the order of
 *    TelemetrySchema::columnNames();
 *  - the `timestamp` column is RELATIVE: 0.000 is the first recorded row;
 *  - cells of columns missing from the row are left empty.
 * The file can be loaded back into the application (FileService::loadCsv()).
 */
#pragma once
#include <fstream>
#include <vector>
#include <string>
#include "IAggregatedDataSubscriber.hpp" 

/**
 * @brief CSV logger subscribed to the DataManager as a "log subscriber" (receives every row at 200 Hz).
 *
 * Created and destroyed by ServiceManager::startLogging() / stopLogging().
 */
class CsvWriter : public IAggregatedDataSubscriber {
public:
    CsvWriter();
    /** @brief Closes the file if still open. */
    ~CsvWriter() override;
    
    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    /**
     * @brief Creates a new file named after the current date/time and writes its header.
     * @param directoryPath Destination folder (must exist).
     * @param columnOrder   Columns to write, in order.
     * @return true if the file was opened.
     */
    bool createFile(const std::string& directoryPath, const std::vector<std::string>& columnOrder);
    /**
     * @brief Writes one row to the file (timestamp converted to relative).
     * @param dataRow Row to write.
     */
    void onAggregatedDataReceived(const DbRow& dataRow) override;
    /** @brief Closes the file. */
    void stop();

    /**
     * @brief Writes a whole set of rows to a CSV file in one go.
     *
     * Utility function (not used by the main flow today: .bin export goes
     * through PlaybackManager::generateCsvFromBin()).
     *
     * @param filePath    Path of the file to create/overwrite.
     * @param columnOrder Columns to write, in order.
     * @param data        Rows to write.
     * @return true if the file was written.
     */
    static bool WriteToFile(
        const std::string& filePath,
        const std::vector<std::string>& columnOrder,
        const std::vector<DbRow>& data
    );

private:
    /** @brief Closes the file if open. */
    void closeFile();
    std::ofstream m_outputFile;               ///< File being written.
    std::vector<std::string> m_columnOrder;   ///< Column order (same as the header).
    bool m_isFirstRow{true};                  ///< Reserved (set but not used).
    double m_recordingStartTime{-1.0};        ///< Absolute timestamp of the first row; -1 = not received yet.
};