#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "Telemetry/file_reading/TelemetryDataset.hpp"

struct FileConfig {
    std::string filePath;
};

class FileService {
public:
    FileService();
    ~FileService();

    bool configure(const FileConfig& config);
    void startLoading();
    bool isLoading() const;
    bool isFinished() const;

    TelemetryDataset getResult();
    const FileConfig& getConfig() const { return m_config; }

private:
    void loadingLoop();
    void loadCsv(const std::string& filePath);
    void loadBin(const std::string& filePath);

    // Registers every column any known layout can produce, so that a recording
    // from an older or newer firmware still lands in a valid slot
    static void prepareColumns(TelemetryDataset& dataset);

    // Fills in the statistic columns that the source did not already provide.
    // A .bin never carries them; a .csv exported by this software does, and
    // reusing those values keeps a bin -> csv -> bin round trip bit-identical.
    void computeMissingStats(TelemetryDataset& dataset);

    FileConfig m_config;
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_isFinished{false};
    std::atomic<bool> m_cancelRequested{false};
    std::thread m_thread;

    TelemetryDataset m_loadedData;
    std::mutex m_dataMutex;
};
