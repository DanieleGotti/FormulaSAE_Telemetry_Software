#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "Telemetry/file_reading/PlaybackManager.hpp" 

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
    
    // Now returns the Native Structs
    std::vector<NativeTelemetryRow> getResult();
    const FileConfig& getConfig() const { return m_config; }

private:
    void loadingLoop();
    void loadCsv(const std::string& filePath);
    void loadBin(const std::string& filePath);
    
    FileConfig m_config;
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_isFinished{false};
    std::atomic<bool> m_cancelRequested{false}; 
    std::thread m_thread;
    
    std::vector<NativeTelemetryRow> m_loadedData;
    std::mutex m_dataMutex;
};