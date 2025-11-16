#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "SerialService.hpp"
#include "NetworkService.hpp"
#include "Telemetry/data_writing/DataManager.hpp"
#include "Telemetry/data_writing/TxtWriter.hpp" 
#include "Telemetry/data_writing/CsvWriter.hpp"
#include "Telemetry/Services/DataAggregatorService.hpp"
#include "Telemetry/file_reading/PlaybackManager.hpp"
#include "Telemetry/Services/FileService.hpp"
#include "../../application_services/SettingsManager.hpp"
#include "../..//utils/IAssetManager.hpp"

typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,
    ACQUISITION_METHOD_NETWORK = 1,
    ACQUISITION_METHOD_FILE = 2
} AcquisitionMethod;

class ServiceManager {
public:
    static void initialize();
    static std::vector<std::string> getAllConnectionOptions();
    static DataManager* getDataManager();
    static DataAggregatorService* getAggregator();
    static PlaybackManager* getPlaybackManager();
    static FileService* getFileService();
    static void setAcquisitionMethod(AcquisitionMethod method);
    static bool configureSerial(const std::string& port, int baudrate);
    static bool configureNetwork(const std::string& ip, int port);
    static bool configureFile(const std::string& filePath);
    static bool startServices();
    static void stopTasks();
    static void cleanup();
    static bool startLogging(const std::string& outputDirectory);
    static void stopLogging();
    static bool isLogging();
    static void resetForNewSession();
    static std::shared_ptr<SettingsManager> getSettingsManager();
    static std::shared_ptr<IAssetManager> getAssetManager();
    static std::string getCurrentLogFileName();

private:
    static AcquisitionMethod m_method;
    static std::shared_ptr<IAssetManager> m_assetManager;
    static std::shared_ptr<SettingsManager> m_settingsManager;
    static std::unique_ptr<SerialService> m_serialService;
    static std::unique_ptr<NetworkService> m_networkService;
    static std::unique_ptr<PlaybackManager> m_playbackManager;
    static std::unique_ptr<FileService> m_fileService;
    static std::unique_ptr<DataAggregatorService> m_aggregatorService; 
    static std::unique_ptr<DataManager> m_dataManager;
    static std::shared_ptr<TxtWriter> m_txtWriter; 
    static std::shared_ptr<CsvWriter> m_csvWriter;
};