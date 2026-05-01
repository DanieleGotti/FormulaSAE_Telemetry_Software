#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "Telemetry/data_writing/DataAggregator.hpp" 

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
    std::vector<DbRow> getResult();
    const FileConfig& getConfig() const { return m_config; }

private:
    void loadingLoop();
    void loadCsv(const std::string& filePath);
    void loadBin(const std::string& filePath);
    
    FileConfig m_config;
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_isFinished{false};
    std::thread m_thread;
    std::vector<DbRow> m_loadedData;
    std::mutex m_dataMutex;
};